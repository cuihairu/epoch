const { defaultTheme } = require("@vuepress/theme-default");

module.exports = {
  lang: "zh-CN",
  title: "Epoch",
  description: "Epoch Actor Runtime 设计与文档",
  theme: defaultTheme({
    navbar: [
      { text: "文档", link: "/" },
      { text: "概念与设计", link: "/guide/concepts.html" },
      { text: "行为与一致性", link: "/guide/behavior.html" },
      { text: "协议与传输", link: "/guide/protocol.html" },
      { text: "语言实现", link: "/guide/languages.html" },
      { text: "GitHub", link: "https://github.com/cuihairu/epoch" }
    ],
    sidebar: {
      "/guide/": [
        {
          text: "概览",
          children: ["/guide/", "/guide/concepts", "/guide/behavior", "/guide/protocol"]
        },
        {
          text: "设计文档",
          children: ["/guide/epoch-actor-design"]
        },
        {
          text: "语言实现",
          children: [
            "/guide/languages",
            "/guide/languages/cpp",
            "/guide/languages/java",
            "/guide/languages/dotnet",
            "/guide/languages/go",
            "/guide/languages/node",
            "/guide/languages/python"
          ]
        }
      ]
    }
  })
};
