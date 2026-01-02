import { viteBundler } from "@vuepress/bundler-vite";
import { defaultTheme } from "@vuepress/theme-default";

export default {
  lang: "zh-CN",
  title: "Epoch",
  description: "Epoch Actor Runtime 设计与文档",
  base: "/epoch/",
  bundler: viteBundler(),
  theme: defaultTheme({
    navbar: [
      { text: "文档", link: "/" },
      { text: "概念与设计", link: "/guide/concepts.html" },
      { text: "行为与一致性", link: "/guide/behavior.html" },
      { text: "协议与传输", link: "/guide/protocol.html" },
      { text: "语言实现", link: "/guide/languages.html" },
      { text: "GitHub", link: "https://github.com/cuihairu/epoch" }
    ],
    sidebar: false
  })
};
