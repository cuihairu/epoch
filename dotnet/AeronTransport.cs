using System.Buffers.Binary;
using System.Runtime.InteropServices;
using System.Text;

namespace Epoch;

public sealed class AeronTransport : ITransport
{
    public sealed record AeronConfig(
        string Channel,
        int StreamId,
        string AeronDirectory,
        int FragmentLimit = 64,
        int OfferMaxAttempts = 10);

    public sealed record AeronStats(
        long SentCount,
        long ReceivedCount,
        long OfferBackPressure,
        long OfferNotConnected,
        long OfferAdminAction,
        long OfferClosed,
        long OfferMaxPosition,
        long OfferFailed);

    private const byte FrameVersion = 1;
    private const int FrameLength = 56;

    private readonly IAeronNative native;
    private readonly IntPtr handle;
    private bool closed;

    public AeronTransport(AeronConfig config, IAeronNative? native = null)
    {
        Config = config;
        this.native = native ?? new AeronNative();
        handle = this.native.Open(config);
    }

    public AeronConfig Config { get; }

    public AeronStats Stats => closed ? new AeronStats(0, 0, 0, 0, 0, 0, 0, 0) : native.Stats(handle);

    public void Send(Message message)
    {
        if (closed)
        {
            throw new InvalidOperationException("Aeron transport closed");
        }
        var frame = EncodeFrame(message);
        native.Send(handle, frame);
    }

    public List<Message> Poll(int max)
    {
        if (closed || max <= 0)
        {
            return new List<Message>();
        }
        var frames = native.Poll(handle, max);
        var output = new List<Message>(frames.Count);
        foreach (var frame in frames)
        {
            output.Add(DecodeFrame(frame));
        }
        return output;
    }

    public void Close()
    {
        if (closed)
        {
            return;
        }
        closed = true;
        native.Close(handle);
    }

    private static byte[] EncodeFrame(Message message)
    {
        var buffer = new byte[FrameLength];
        buffer[0] = FrameVersion;
        buffer[1] = message.Qos;
        BinaryPrimitives.WriteInt64LittleEndian(buffer.AsSpan(8, 8), message.Epoch);
        BinaryPrimitives.WriteInt64LittleEndian(buffer.AsSpan(16, 8), message.ChannelId);
        BinaryPrimitives.WriteInt64LittleEndian(buffer.AsSpan(24, 8), message.SourceId);
        BinaryPrimitives.WriteInt64LittleEndian(buffer.AsSpan(32, 8), message.SourceSeq);
        BinaryPrimitives.WriteInt64LittleEndian(buffer.AsSpan(40, 8), message.SchemaId);
        BinaryPrimitives.WriteInt64LittleEndian(buffer.AsSpan(48, 8), message.Payload);
        return buffer;
    }

    private static Message DecodeFrame(byte[] buffer)
    {
        if (buffer.Length < FrameLength)
        {
            throw new InvalidOperationException("Aeron frame too short");
        }
        if (buffer[0] != FrameVersion)
        {
            throw new InvalidOperationException("Unsupported Aeron frame version");
        }
        var epoch = BinaryPrimitives.ReadInt64LittleEndian(buffer.AsSpan(8, 8));
        var channelId = BinaryPrimitives.ReadInt64LittleEndian(buffer.AsSpan(16, 8));
        var sourceId = BinaryPrimitives.ReadInt64LittleEndian(buffer.AsSpan(24, 8));
        var sourceSeq = BinaryPrimitives.ReadInt64LittleEndian(buffer.AsSpan(32, 8));
        var schemaId = BinaryPrimitives.ReadInt64LittleEndian(buffer.AsSpan(40, 8));
        var payload = BinaryPrimitives.ReadInt64LittleEndian(buffer.AsSpan(48, 8));
        var qos = buffer[1];
        return new Message(epoch, channelId, sourceId, sourceSeq, schemaId, qos, payload);
    }

    public interface IAeronNative
    {
        IntPtr Open(AeronConfig config);
        void Send(IntPtr transport, byte[] frame);
        List<byte[]> Poll(IntPtr transport, int max);
        AeronStats Stats(IntPtr transport);
        void Close(IntPtr transport);
    }

    private sealed class AeronNative : IAeronNative
    {
        public IntPtr Open(AeronConfig config)
        {
            var channelPtr = Marshal.StringToHGlobalAnsi(config.Channel);
            var dirPtr = string.IsNullOrEmpty(config.AeronDirectory)
                ? IntPtr.Zero
                : Marshal.StringToHGlobalAnsi(config.AeronDirectory);
            try
            {
                var nativeConfig = new AeronConfigNative
                {
                    Channel = channelPtr,
                    StreamId = config.StreamId,
                    AeronDirectory = dirPtr,
                    FragmentLimit = config.FragmentLimit,
                    OfferMaxAttempts = config.OfferMaxAttempts
                };
                var error = new StringBuilder(256);
                var handle = NativeMethods.epoch_aeron_open(ref nativeConfig, error, (UIntPtr)error.Capacity);
                if (handle == IntPtr.Zero)
                {
                    throw new InvalidOperationException(error.ToString());
                }
                return handle;
            }
            finally
            {
                Marshal.FreeHGlobal(channelPtr);
                if (dirPtr != IntPtr.Zero)
                {
                    Marshal.FreeHGlobal(dirPtr);
                }
            }
        }

        public void Send(IntPtr transport, byte[] frame)
        {
            var error = new StringBuilder(256);
            var result = NativeMethods.epoch_aeron_send(
                transport, frame, (UIntPtr)frame.Length, error, (UIntPtr)error.Capacity);
            if (result < 0)
            {
                throw new InvalidOperationException(error.ToString());
            }
        }

        public List<byte[]> Poll(IntPtr transport, int max)
        {
            var error = new StringBuilder(256);
            var buffer = new byte[max * FrameLength];
            var result = NativeMethods.epoch_aeron_poll(
                transport, buffer, (UIntPtr)max, out var count, error, (UIntPtr)error.Capacity);
            if (result < 0)
            {
                throw new InvalidOperationException(error.ToString());
            }
            var total = (int)count;
            var frames = new List<byte[]>(total);
            for (var i = 0; i < total; i++)
            {
                var frame = new byte[FrameLength];
                Buffer.BlockCopy(buffer, i * FrameLength, frame, 0, FrameLength);
                frames.Add(frame);
            }
            return frames;
        }

        public AeronStats Stats(IntPtr transport)
        {
            var stats = new AeronStatsNative();
            if (NativeMethods.epoch_aeron_stats(transport, ref stats) < 0)
            {
                return new AeronStats(0, 0, 0, 0, 0, 0, 0, 0);
            }
            return new AeronStats(
                stats.SentCount,
                stats.ReceivedCount,
                stats.OfferBackPressure,
                stats.OfferNotConnected,
                stats.OfferAdminAction,
                stats.OfferClosed,
                stats.OfferMaxPosition,
                stats.OfferFailed);
        }

        public void Close(IntPtr transport)
        {
            if (transport != IntPtr.Zero)
            {
                NativeMethods.epoch_aeron_close(transport);
            }
        }
    }

    [StructLayout(LayoutKind.Sequential)]
    private struct AeronConfigNative
    {
        public IntPtr Channel;
        public int StreamId;
        public IntPtr AeronDirectory;
        public int FragmentLimit;
        public int OfferMaxAttempts;
    }

    [StructLayout(LayoutKind.Sequential)]
    private struct AeronStatsNative
    {
        public long SentCount;
        public long ReceivedCount;
        public long OfferBackPressure;
        public long OfferNotConnected;
        public long OfferAdminAction;
        public long OfferClosed;
        public long OfferMaxPosition;
        public long OfferFailed;
    }

    private static class NativeMethods
    {
        [DllImport("epoch_aeron", CallingConvention = CallingConvention.Cdecl)]
        internal static extern IntPtr epoch_aeron_open(
            ref AeronConfigNative config,
            StringBuilder error,
            UIntPtr errorLen);

        [DllImport("epoch_aeron", CallingConvention = CallingConvention.Cdecl)]
        internal static extern int epoch_aeron_send(
            IntPtr transport,
            byte[] frame,
            UIntPtr frameLen,
            StringBuilder error,
            UIntPtr errorLen);

        [DllImport("epoch_aeron", CallingConvention = CallingConvention.Cdecl)]
        internal static extern int epoch_aeron_poll(
            IntPtr transport,
            byte[] frames,
            UIntPtr frameCapacity,
            out UIntPtr outCount,
            StringBuilder error,
            UIntPtr errorLen);

        [DllImport("epoch_aeron", CallingConvention = CallingConvention.Cdecl)]
        internal static extern int epoch_aeron_stats(
            IntPtr transport,
            ref AeronStatsNative stats);

        [DllImport("epoch_aeron", CallingConvention = CallingConvention.Cdecl)]
        internal static extern void epoch_aeron_close(IntPtr transport);
    }
}
