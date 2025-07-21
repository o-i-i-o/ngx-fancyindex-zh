所有对该项目的重要更改都将记录在此文件中。

## [未发布]
## [以下为原版日志的简单翻译]

## [0.5.2] - 2021-10-28
### 修复
- 正确转义文件名，确保文件名永远不会被渲染为HTML。（补丁由 Anthony Ryan <<anthonyryan1@gmail.com>> 提供，[#128](https://github.com/aperezdc/ngx-fancyindex/pull/128)。）

## [0.5.1] - 2020-10-26
### 修复
- 正确处理 `fancyindex_header` 和 `fancyindex_footer` 的可选第二个参数
  ([#117](https://github.com/aperezdc/ngx-fancyindex/issues/117))。

## [0.5.0] - 2020-10-24
### 新增
- 新选项 `fancyindex_show_dotfiles`。（路径由 Joshua Shaffer <<joshua.shaffer@icmrl.net>> 提供。）
- `fancyindex_header` 和 `fancyindex_footer` 选项现在通过 `local` 标志正确支持本地文件。（补丁由 JoungKyun Kim <<joungkyun@gmail.com>> 和 Adrián Pérez <<aperez@igalia.com>> 提供。）

### 变更
- 改进了目录条目排序的性能，对于包含数千个文件的目录，这应该非常明显。（补丁由 [Yuxiang Zhang](https://github.com/z4yx) 提供。）
- 模块支持的最低 Nginx 版本现在是 0.8.x。

### 修复
- 当模块使用旧版本的 Nginx 构建时，正确转义目录条目名称中的方括号。（补丁由 Adrián Pérez <<aperez@igalia.com>> 提供。）
- 修复使用 [nginx-auth-ldap](https://github.com/kvspb/nginx-auth-ldap) 模块时不显示目录条目列表的问题。（补丁由 JoungKyun Kim <<joungkyun@gmail.com>> 提供。）

## [0.4.4] - 2020-02-19
### 新增
- 新选项 `fancyindex_hide_parent_dir`，它禁止在列表中生成指向父目录的链接。（补丁由 Kawai Ryota <<admin@mail.kr-kp.com>> 提供。）

### 变更
- 现在每个表格行由新行（实际上是 `CRLF` 序列）分隔，这使得使用简单文本工具解析输出更加容易。（补丁由 Anders Trier <<anders.trier.olesen@gmail.com>> 提供。）
- 对 README 文件进行了一些更正和补充。（补丁由 Nicolas Carpi <<nicolas.carpi@curie.fr>> 和 David Beitey <<david@davidjb.com>> 提供。）

### 修复
- 在模板中的表格排序器 URL 中使用正确的 `&` 字符引用（补丁由 David Beitey <<david@davidjb.com>> 提供。）
- 当文件名用作 URI 组件时正确编码。

## [0.4.3] - 2018-07-03
### 新增
- 表格单元格现在具有类名，这允许更好的 CSS 样式。（补丁由 qjqqyy <<gyula@nyirfalvi.hu>> 提供。）
- 测试套件现在可以解析和检查模块返回的 HTML 中的元素，这要归功于 [pup](https://github.com/EricChiang/pup) 工具。

### 修复
- 按文件大小排序现在正常工作。（补丁由 qjqqyy <<gyula@nyirfalvi.hu>> 提供。）

## [0.4.2] - 2017-08-19
### 变更
- 从默认模板生成的 HTML 现在是正确的 HTML5，并且应该通过验证 (#52)。
- 当使用 `fancyindex_exact_size off` 时，文件大小现在具有小数位。（补丁由 Anders Trier <<anders.trier.olesen@gmail.com>> 提供。）
- 对 `README.rst` 进行了多次更新（补丁由 Danila Vershinin <<ciapnz@gmail.com>>、Iulian Onofrei、Lilian Besson 和 Nick Geoghegan <<nick@nickgeoghegan.net>> 提供。）

### 修复
- 按文件大小排序现在对于包含大于 `INT_MAX` 大小文件的目录也能正常工作。(#74，修复建议由 Chris Young 提供。)
- 未能声明 UTF-8 编码的自定义页眉不再导致浏览器错误渲染表格页眉箭头 (#50)。
- 修复打开包含空文件的目录时的段错误 (#61，补丁由 Catgirl <<cat@wolfgirl.org>> 提供。)

## [0.4.1] - 2016-08-18
### 新增
- 新的 `fancyindex_directories_first` 配置指令（默认启用），允许设置目录是否在其他文件之前排序。（补丁由 Luke Zapart <<luke@zapart.org>> 提供。）

### 修复
- 修复使用 fancyindex 模块时索引文件不工作的问题 (#46)。


## [0.4.0] - 2016-06-08
### 新增
- 该模块现在可以作为 [动态模块](https://www.nginx.com/resources/wiki/extending/converting/) 构建。（补丁由 Róbert Nagy <<vrnagy@gmail.com>> 提供。）
- 新的配置指令 `fancyindex_show_path`，允许隐藏包含当前路径的 `<h1>` 页眉。（补丁由 Thomas P. <<tpxp@live.fr>> 提供。）

### 变更
- 列表中的目录和文件链接现在具有 title="..." 属性。（补丁由 `@janglapuk` <<trusdi.agus@gmail.com>> 提供。）

### 修复
- 修复与 `ngx_pagespeed` 一起使用时请求挂起的问题。（补丁由 Otto van der Schaaf <<oschaaf@we-amp.com>> 提供。）


## [0.3.6] - 2016-01-26
### 新增
- 新功能：允许使用 `fancyindex_hide_symlinks` 配置指令过滤掉符号链接。（想法和原型补丁由 Thomas Wemm 提供。）
- 新功能：允许使用 `fancyindex_time_format` 配置指令指定时间戳的格式。（想法由 Xiao Meng <<novoreorx@gmail.com>> 提出。）

### 变更
- 顶级目录中的列表不会在列表的第一个元素生成 "父目录" 链接。（补丁由 Thomas P. 提供。）

### 修复
- 修复嵌套位置内 `fancyindex_css_href` 设置的传播和覆盖问题。
- 代码中的小更改，以允许在 Windows 上使用 Visual Studio 2013 干净地构建。（补丁由 Y. Yuan <<yzwduck@gmail.com>> 提供。）


## [0.3.5] - 2015-02-19
### 新增
- 新功能：允许使用 `fancyindex_default_sort` 配置指令设置默认排序标准。（补丁由 Алексей Урбанский 提供。）
- 新功能：允许使用 `fancyindex_name_length` 配置指令更改文件名的最大长度。（补丁由 Martin Herkt 提供。）

### 变更
- 将 `NEWS.rst` 重命名为 `CHANGELOG.md`，这遵循了 [Keep a Change Log](http://keepachangelog.com/) 的建议。
- 没有 `http_addition_module` 配置 Nginx 将在配置期间生成警告，因为 `fancyindex_footer` 和 `fancyindex_header` 指令需要它。


## [0.3.4] - 2014-09-03

### 新增
- 现在在生成的 HTML 中定义了视口，这对移动设备更友好。

### 变更
- 奇偶行样式使用 :nth-child() 移动到 CSS。这使得提供给客户端的 HTML 更小。


## [0.3.3] - 2013-10-25

### 新增
- 新功能：默认模板中的表格页眉现在可点击，以设置索引条目的排序标准和方向。（https://github.com/aperezdc/ngx-fancyindex/issues/7）


## [0.3.2] - 2013-06-05

### 修复
- 解决了会使某些客户端永远停滞的错误。
- 改进了非内置页眉/页脚的子请求处理。


## [0.3.1] - 2011-04-04

### 新增
- `NEWS.rst` 文件，用作变更日志。


[未发布]: https://github.com/aperezdc/ngx-fancyindex/compare/v0.5.2...HEAD
[0.5.2]: https://github.com/aperezdc/ngx-fancyindex/compare/v0.5.1...v0.5.2
[0.5.1]: https://github.com/aperezdc/ngx-fancyindex/compare/v0.5.0...v0.5.1
[0.5.0]: https://github.com/aperezdc/ngx-fancyindex/compare/v0.4.4...v0.5.0
[0.4.4]: https://github.com/aperezdc/ngx-fancyindex/compare/v0.4.3...v0.4.4
[0.4.3]: https://github.com/aperezdc/ngx-fancyindex/compare/v0.4.2...v0.4.3
[0.4.2]: https://github.com/aperezdc/ngx-fancyindex/compare/v0.4.1...v0.4.2
[0.4.1]: https://github.com/aperezdc/ngx-fancyindex/compare/v0.4.0...v0.4.1
[0.4.0]: https://github.com/aperezdc/ngx-fancyindex/compare/v0.3.6...v0.4.0
[0.3.6]: https://github.com/aperezdc/ngx-fancyindex/compare/v0.3.5...v0.3.6
[0.3.5]: https://github.com/aperezdc/ngx-fancyindex/compare/v0.3.4...v0.3.5
[0.3.4]: https://github.com/aperezdc/ngx-fancyindex/compare/v0.3.3...v0.3.4
[0.3.3]: https://github.com/aperezdc/ngx-fancyindex/compare/v0.3.2...v0.3.3
[0.3.2]: https://github.com/aperezdc/ngx-fancyindex/compare/v0.3.1...v0.3.2
[0.3.1]: https://github.com/aperezdc/ngx-fancyindex/compare/v0.3...v0.3.1
