Nginx Fancy Index 模块
=====================
## 机器翻译，当产生疑问时，请查阅英文原版说明
  - // .. image:: https://travis-ci.com/aperezdc/ngx-fancyindex.svg?branch=master
  - //   :target: https://travis-ci.com/aperezdc/ngx-fancyindex
  - //  :alt: 构建状态

.. 正文::

Fancy Index 模块使得生成文件列表成为可能，就像内置的 `autoindex <http://wiki.nginx.org/NginxHttpAutoindexModule>`__ 模块一样，但增加了一些样式。这是可能的，因为该模块允许对生成的内容进行一定程度的自定义：

* 自定义页眉。可以是本地的或远程存储的。
* 自定义页脚。可以是本地的或远程存储的。
* 添加您自己的 CSS 样式规则。
* 允许选择按名称（默认）、修改时间或大小排序；升序（默认）或降序。

此模块旨在与 Nginx_ 一起使用，Nginx 是由 `Igor Sysoev <http://sysoev.ru>`__ 编写的高性能开源 Web 服务器。


需求
====

CentOS 7
~~~~~~~

对于使用 `官方稳定版 <https://www.nginx.com/resources/wiki/start/topics/tutorials/install/>`__ Nginx 仓库的用户，提供了带有动态模块的 `额外包仓库 <https://www.getpagespeed.com/redhat>`__，其中包含了 fancyindex。

直接安装：

::

    yum install https://extras.getpagespeed.com/redhat/7/x86_64/RPMS/nginx-module-fancyindex-1.12.0.0.4.1-1.el7.gps.x86_64.rpm

或者，先添加额外仓库（用于未来更新）然后安装模块：

::

    yum install nginx-module-fancyindex

然后在 `/etc/nginx/nginx.conf` 中使用以下行加载模块：

::

   load_module "modules/ngx_http_fancyindex_module.so";

其他平台
~~~~~~~~

在大多数其他情况下，您将需要 Nginx_ 的源代码。任何从 0.8 系列开始的版本都应该可以工作。

为了使用 ``fancyindex_header_`` 和 ``fancyindex_footer_`` 指令，您还需要将 `ngx_http_addition_module <https://nginx.org/en/docs/http/ngx_http_addition_module.html>`_ 内置到 Nginx 中。


构建
====

1. 解压 Nginx_ 源代码：

::

    $ tar -zxvf - nginx-?.?.?.tar.gz 

2. 解压 fancy indexing 模块的源代码：

::

    $ tar -zxvf nginx-fancyindex-?.?.?.tar.gz 

3. 进入包含 Nginx_ 源代码的目录，运行配置脚本，使用所需的选项，并确保添加一个 ``--add-module`` 标志，指向包含 fancy indexing 模块源代码的目录：

::

    $ cd nginx-?.?.?
    $ ./configure --add-module=../nginx-fancyindex-?.?.? \
       [--with-http_addition_module] [其他所需选项]

   从版本 0.4.0 开始，该模块也可以作为 `动态模块 <https://www.nginx.com/resources/wiki/extending/converting/>`_ 构建，使用 ``--add-dynamic-module=…`` 代替，并在配置文件中使用 ``load_module "modules/ngx_http_fancyindex_module.so";``

4. 构建并安装软件：

::

    $ make

   然后，以 ``root`` 身份：

::

    # make install

5. 通过使用模块的配置指令_配置 Nginx_。


示例
====

您可以通过在 Nginx_ 配置文件的 ``server`` 部分添加以下行来测试默认的内置样式：

::

  location / {
    fancyindex on;              # 启用 fancy 索引。
    fancyindex_exact_size off;  # 输出人类可读的文件大小。
  }


主题
~~~~

以下主题展示了使用该模块可以实现的自定义程度：

* `主题 <https://github.com/TheInsomniac/Nginx-Fancyindex-Theme>`__ 由 `@TheInsomniac <https://github.com/TheInsomniac>`__ 创建。使用自定义页眉和页脚。
* `主题 <https://github.com/Naereen/Nginx-Fancyindex-Theme>`__ 由 `@Naereen <https://github.com/Naereen/>`__ 创建。使用自定义页眉和页脚，页眉包含使用 JavaScript 按文件名过滤的搜索字段。
* `主题 <https://github.com/fraoustin/Nginx-Fancyindex-Theme>`__ 由 `@fraoustin <https://github.com/fraoustin>`__ 创建。使用 Material Design 元素的响应式主题。
* `主题 <https://github.com/alehaa/nginx-fancyindex-flat-theme>`__ 由 `@alehaa <https://github.com/alehaa>`__ 创建。基于 Bootstrap 4 和 FontAwesome 的简单、扁平主题。
* `主题 <https://github.com/o-i-i-o/fancyindex-theme-zh>`__ 由 `@o-i-i-o <https://github.com/o-i-i-o>`__ 我自己修改的简单汉化主题（推荐）。

指令
====

fancyindex
~~~~~~~~~
:Syntax: *fancyindex* [*on* | *off*]
:Default: fancyindex off
:Context: http, server, location
:Description:
  启用或禁用 fancy 目录索引。

fancyindex_default_sort
~~~~~~~~~~~~~~~~~~~~~~~
:Syntax: *fancyindex_default_sort* [*name* | *size* | *date* | *name_desc* | *size_desc* | *date_desc*]
:Default: fancyindex_default_sort name
:Context: http, server, location
:Description:
  定义默认的排序标准。

fancyindex_directories_first
~~~~~~~~~~~~~~~~~~~~~~~~~~~~
:Syntax: *fancyindex_directories_first* [*on* | *off*]
:Default: fancyindex_directories_first on
:Context: http, server, location
:Description:
  如果启用（默认设置），将目录分组在一起并在所有常规文件之前排序。如果禁用，目录将与文件一起排序。

fancyindex_css_href
~~~~~~~~~~~~~~~~~~~
:Syntax: *fancyindex_css_href uri*
:Default: fancyindex_css_href ""
:Context: http, server, location
:Description:
  允许在生成的列表中插入指向 CSS 样式表的链接。提供的 *uri* 参数将按原样插入到 ``<link>`` HTML 标签中。该链接插入在内置 CSS 规则之后，因此您可以覆盖默认样式。

fancyindex_exact_size
~~~~~~~~~~~~~~~~~~~~~
:Syntax: *fancyindex_exact_size* [*on* | *off*]
:Default: fancyindex_exact_size on
:Context: http, server, location
:Description:
  定义如何在目录列表中表示文件大小；精确表示，或四舍五入到千字节、兆字节和千兆字节。

fancyindex_name_length
~~~~~~~~~~~~~~~~~~~~~~
:Syntax: *fancyindex_name_length length*
:Default: fancyindex_name_length 50
:Context: http, server, location
:Description:
  定义最大文件名长度限制（以字节为单位）。

fancyindex_footer
~~~~~~~~~~~~~~~~~
:Syntax: *fancyindex_footer path* [*subrequest* | *local*]
:Default: fancyindex_footer ""
:Context: http, server, location
:Description:
  指定应插入到目录列表底部的文件。如果设置为空字符串，将发送模块提供的默认页脚。可选参数指示 *path* 是被视为使用 *subrequest* 加载的 URI（默认），还是指 *local* 文件。

.. note:: 使用此指令需要将 ngx_http_addition_module_ 内置到 Nginx 中。

.. warning:: 插入自定义页眉/页脚时，将发出子请求，因此可能使用任何 URL 作为它们的源。虽然它可以与外部 URL 一起使用，但仅支持使用内部 URL。
