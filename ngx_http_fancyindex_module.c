/*
 * ngx_http_fancyindex_module.c
 * 版权所有 © 2007-2016 Adrian Perez <aperez@igalia.com>
 *
 * 用于nginx autoindex的美化索引的模块。与标准nginx autoindex模块的特性和区别：
 *
 *  - 输出是表格形式，而非带有嵌入式<a>链接的<pre>元素。
 *  - 可以为每个生成的目录列表添加页眉和页脚。
 *  - 如果未配置自定义页眉和/或页脚，则生成默认的页眉和/或页脚。
 *    用于页眉和页脚的文件只能是本地路径名（即不能插入子请求的结果）。
 *  - 生成正确的HTML：应同时符合XHTML 1.0 Strict和HTML 4.01标准。
 *
 * 基本功能很大程度上基于标准nginx autoindex模块，该模块与nginx的大部分组件一样，
 * 由Igor Sysoev开发。
 *
 * 根据BSD许可证条款分发。
 */

#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>
#include <ngx_log.h>

#include "template.h"

/* 编译器特定优化 */
#if defined(__GNUC__) && (__GNUC__ >= 3)
# define ngx_force_inline __attribute__((__always_inline__))
#else /* !__GNUC__ */
# define ngx_force_inline
#endif /* __GNUC__ */


/* 短格式星期几 */
static const char *short_weekday[] = {
    "周一", "周二", "周三", "周四", "周五", "周六", "周日",
};
/* 长格式星期几 */
static const char *long_weekday[] = {
    "星期一", "星期二", "星期三", "星期四", "星期五", "星期日",
};
/* 短格式月份 */
static const char *short_month[] = {
    "一月", "二月", "三月", "四月", "五月", "六月",
    "七月", "八月", "九月", "十月", "十一月", "十二月",
};
/* 长格式月份 */
static const char *long_month[] = {
    "一月", "二月", "三月", "四月", "五月", "六月", "七月",
    "八月", "九月", "十月", "十一月", "十二月",
};


/* 日期时间格式定义宏 */
#define DATETIME_FORMATS(F_, t) \
    F_ ('a',  3, "%3s",  short_weekday[((t)->ngx_tm_wday + 6) % 7]) \
    F_ ('A',  9, "%s",   long_weekday [((t)->ngx_tm_wday + 6) % 7]) \
    F_ ('b',  3, "%3s",  short_month[(t)->ngx_tm_mon - 1]         ) \
    F_ ('B',  9, "%s",   long_month [(t)->ngx_tm_mon - 1]         ) \
    F_ ('d',  2, "%02d", (t)->ngx_tm_mday                         ) \
    F_ ('e',  2, "%2d",  (t)->ngx_tm_mday                         ) \
    F_ ('F', 10, "%d-%02d-%02d",                                    \
                  (t)->ngx_tm_year,                                 \
                  (t)->ngx_tm_mon,                                  \
                  (t)->ngx_tm_mday                                ) \
    F_ ('H',  2, "%02d", (t)->ngx_tm_hour                         ) \
    F_ ('I',  2, "%02d", ((t)->ngx_tm_hour % 12) + 1              ) \
    F_ ('k',  2, "%2d",  (t)->ngx_tm_hour                         ) \
    F_ ('l',  2, "%2d",  ((t)->ngx_tm_hour % 12) + 1              ) \
    F_ ('m',  2, "%02d", (t)->ngx_tm_mon                          ) \
    F_ ('M',  2, "%02d", (t)->ngx_tm_min                          ) \
    F_ ('p',  2, "%2s",  (((t)->ngx_tm_hour < 12) ? "上午" : "下午")  ) \
    F_ ('P',  2, "%2s",  (((t)->ngx_tm_hour < 12) ? "上午" : "下午")  ) \
    F_ ('r', 11, "%02d:%02d:%02d %2s",                              \
                 ((t)->ngx_tm_hour % 12) + 1,                       \
                 (t)->ngx_tm_min,                                   \
                 (t)->ngx_tm_sec,                                   \
                 (((t)->ngx_tm_hour < 12) ? "上午" : "下午")          ) \
    F_ ('R',  5, "%02d:%02d", (t)->ngx_tm_hour, (t)->ngx_tm_min   ) \
    F_ ('S',  2, "%02d", (t)->ngx_tm_sec                          ) \
    F_ ('T',  8, "%02d:%02d:%02d",                                  \
                 (t)->ngx_tm_hour,                                  \
                 (t)->ngx_tm_min,                                   \
                 (t)->ngx_tm_sec                                  ) \
    F_ ('u',  1, "%1d", (((t)->ngx_tm_wday + 6) % 7) + 1          ) \
    F_ ('w',  1, "%1d", ((t)->ngx_tm_wday + 6) % 7                ) \
    F_ ('y',  2, "%02d", (t)->ngx_tm_year % 100                   ) \
    F_ ('Y',  4, "%04d", (t)->ngx_tm_year                         )


/* 计算时间格式字符串所需的缓冲区大小 */
static size_t
ngx_fancyindex_timefmt_calc_size (const ngx_str_t *fmt)
{
/* 日期时间格式转换的case宏 */
#define DATETIME_CASE(letter, fmtlen, fmt, ...) \
        case letter: result += (fmtlen); break;

    size_t i, result = 0;
    for (i = 0; i < fmt->len; i++) {
        if (fmt->data[i] == '%') {
            if (++i >= fmt->len) {
                result++;
                break;
            }
            switch (fmt->data[i]) {
                DATETIME_FORMATS(DATETIME_CASE,)
                default:
                    result++;
            }
        } else {
            result++;
        }
    }
    return result;

#undef DATETIME_CASE
}


/* 格式化时间为指定格式的字符串 */
static u_char*
ngx_fancyindex_timefmt (u_char *buffer, const ngx_str_t *fmt, const ngx_tm_t *tm)
{
#define DATETIME_CASE(letter, fmtlen, fmt, ...) \
        case letter: buffer = ngx_snprintf(buffer, fmtlen, fmt, ##__VA_ARGS__); break;

    size_t i;
    for (i = 0; i < fmt->len; i++) {
        if (fmt->data[i] == '%') {
            if (++i >= fmt->len) {
                *buffer++ = '%';
                break;
            }
            switch (fmt->data[i]) {
                DATETIME_FORMATS(DATETIME_CASE, tm)
                default:
                    *buffer++ = fmt->data[i];
            }
        } else {
            *buffer++ = fmt->data[i];
        }
    }
    return buffer;

#undef DATETIME_CASE
}

/* 页眉页脚配置结构体 */
typedef struct {
    ngx_str_t path;    /* 页眉/页脚文件路径 */
    ngx_str_t local;   /* 本地页眉/页脚内容 */
} ngx_fancyindex_headerfooter_conf_t;

/**
 * fancyindex模块的配置结构体。模块中定义的配置指令用于填充此结构体的成员。
 */
typedef struct {
    ngx_flag_t enable;         /**< 模块是否启用。 */
    ngx_uint_t default_sort;   /**< 默认排序标准。 */
    ngx_flag_t dirs_first;     /**< 排序时将目录分组在一起显示在前面 */
    ngx_flag_t localtime;      /**< 文件修改时间以本地时间显示 */
    ngx_flag_t exact_size;     /**< 文件大小始终以字节显示 */
    ngx_uint_t name_length;    /**< 文件名的最大长度（字节） */
    ngx_flag_t hide_symlinks;  /**< 在列表中隐藏符号链接 */
    ngx_flag_t show_path;      /**< 是否在标题后显示路径 + '</h1>' */
    ngx_flag_t hide_parent;    /**< 隐藏上级目录 */
    ngx_flag_t show_dot_files; /**< 显示以点开头的文件 */

    ngx_str_t  css_href;       /**< CSS样式表链接，无则为空 */
    ngx_str_t  time_format;    /**< 文件时间戳的格式 */

    ngx_array_t *ignore;       /**< 列表中要忽略的文件列表 */

    ngx_fancyindex_headerfooter_conf_t header;
    ngx_fancyindex_headerfooter_conf_t footer;
} ngx_http_fancyindex_loc_conf_t;

/* 按名称升序排序 */
#define NGX_HTTP_FANCYINDEX_SORT_CRITERION_NAME       0
/* 按大小升序排序 */
#define NGX_HTTP_FANCYINDEX_SORT_CRITERION_SIZE       1
/* 按日期升序排序 */
#define NGX_HTTP_FANCYINDEX_SORT_CRITERION_DATE       2
/* 按名称降序排序 */
#define NGX_HTTP_FANCYINDEX_SORT_CRITERION_NAME_DESC  3
/* 按大小降序排序 */
#define NGX_HTTP_FANCYINDEX_SORT_CRITERION_SIZE_DESC  4
/* 按日期降序排序 */
#define NGX_HTTP_FANCYINDEX_SORT_CRITERION_DATE_DESC  5

/* 排序标准枚举配置 */
static ngx_conf_enum_t ngx_http_fancyindex_sort_criteria[] = {
    { ngx_string("name"), NGX_HTTP_FANCYINDEX_SORT_CRITERION_NAME },
    { ngx_string("size"), NGX_HTTP_FANCYINDEX_SORT_CRITERION_SIZE },
    { ngx_string("date"), NGX_HTTP_FANCYINDEX_SORT_CRITERION_DATE },
    { ngx_string("name_desc"), NGX_HTTP_FANCYINDEX_SORT_CRITERION_NAME_DESC },
    { ngx_string("size_desc"), NGX_HTTP_FANCYINDEX_SORT_CRITERION_SIZE_DESC },
    { ngx_string("date_desc"), NGX_HTTP_FANCYINDEX_SORT_CRITERION_DATE_DESC },
    { ngx_null_string, 0 }
};

/* 页眉页脚类型枚举 */
enum {
    NGX_HTTP_FANCYINDEX_HEADERFOOTER_SUBREQUEST,  /* 子请求页眉/页脚 */
    NGX_HTTP_FANCYINDEX_HEADERFOOTER_LOCAL,       /* 本地页眉/页脚 */
};

/* 判断页眉/页脚类型（子请求或本地文件） */
static ngx_uint_t
headerfooter_kind(const ngx_str_t *value)
{
    static const struct {
        ngx_str_t name;
        ngx_uint_t value;
    } values[] = {
        { ngx_string("subrequest"), NGX_HTTP_FANCYINDEX_HEADERFOOTER_SUBREQUEST },
        { ngx_string("local"), NGX_HTTP_FANCYINDEX_HEADERFOOTER_LOCAL },
    };

    unsigned i;

    for (i = 0; i < sizeof(values) / sizeof(values[0]); i++) {
        if (value->len == values[i].name.len &&
            ngx_strcasecmp(value->data, values[i].name.data) == 0)
        {
            return values[i].value;
        }
    }

    return NGX_CONF_UNSET_UINT;
}

/* 设置页眉/页脚配置 */
static char*
ngx_fancyindex_conf_set_headerfooter(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
    ngx_fancyindex_headerfooter_conf_t *item =
        (void*) (((char*) conf) + cmd->offset);
    ngx_str_t *values = cf->args->elts;

    if (item->path.data)
        return "is duplicate";

    item->path = values[1];

    /* 路径类型。默认为"subrequest"。 */
    ngx_uint_t kind = NGX_HTTP_FANCYINDEX_HEADERFOOTER_SUBREQUEST;
    if (cf->args->nelts == 3) {
        kind = headerfooter_kind(&values[2]);
        if (kind == NGX_CONF_UNSET_UINT) {
            ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                               "unknown header/footer kind \"%V\"", &values[2]);
            return NGX_CONF_ERROR;
        }
    }

    if (kind == NGX_HTTP_FANCYINDEX_HEADERFOOTER_LOCAL) {
        ngx_file_t file;
        ngx_file_info_t fi;
        ssize_t n;

        ngx_memzero(&file, sizeof(ngx_file_t));
        file.log = cf->log;
        file.fd = ngx_open_file(item->path.data, NGX_FILE_RDONLY, 0, 0);
        if (file.fd == NGX_INVALID_FILE) {
            ngx_conf_log_error(NGX_LOG_EMERG, cf, ngx_errno,
                               "cannot open file \"%V\"", &values[1]);
            return NGX_CONF_ERROR;
        }

        if (ngx_fd_info(file.fd, &fi) == NGX_FILE_ERROR) {
            ngx_close_file(file.fd);
            ngx_conf_log_error(NGX_LOG_EMERG, cf, ngx_errno,
                               "cannot get info for file \"%V\"", &values[1]);
            return NGX_CONF_ERROR;
        }

        item->local.len = ngx_file_size(&fi);
        item->local.data = ngx_pcalloc(cf->pool, item->local.len + 1);
        if (item->local.data == NULL) {
            ngx_close_file(file.fd);
            return NGX_CONF_ERROR;
        }

        n = item->local.len;
        while (n > 0) {
            ssize_t r = ngx_read_file(&file,
                                      item->local.data + file.offset,
                                      n,
                                      file.offset);
            if (r == NGX_ERROR) {
                ngx_close_file(file.fd);
                ngx_conf_log_error(NGX_LOG_EMERG, cf, ngx_errno,
                                   "cannot read file \"%V\"", &values[1]);
                return NGX_CONF_ERROR;
            }

            n -= r;
        }
        item->local.data[item->local.len] = '\0';
    }

    return NGX_CONF_OK;
}

#define NGX_HTTP_FANCYINDEX_PREALLOCATE  50


/**
 * 计算以NULL结尾的字符串长度。需要记住从sizeof结果中减去1，这有点麻烦。
 */
#define ngx_sizeof_ssz(_s)  (sizeof(_s) - 1)

/**
 * 计算静态分配数组的长度
 */
#define DIM(x) (sizeof(x)/sizeof(*(x)))

/**
 * 复制静态零终止字符串。用于将模板字符串片段输出到临时缓冲区。
 */
#define ngx_cpymem_ssz(_p, _t) \
	(ngx_cpymem((_p), (_t), sizeof(_t) - 1))

/**
 * 复制ngx_str_t类型。
 */
#define ngx_cpymem_str(_p, _s) \
	(ngx_cpymem((_p), (_s).data, (_s).len))

/**
 * 检查特定值中是否设置了特定位。
 */
#define ngx_has_flag(_where, _what) \
	(((_where) & (_what)) == (_what))




/* 目录条目结构体 */
typedef struct {
    ngx_str_t      name;        /* 文件名 */
    size_t         utf_len;     /* UTF-8编码的文件名长度 */
    ngx_uint_t     escape;      /* URL转义字符数 */
    ngx_uint_t     escape_html; /* HTML转义字符数 */
    ngx_uint_t     dir;         /* 是否为目录 */
    time_t         mtime;       /* 修改时间 */
    off_t          size;        /* 文件大小 */
} ngx_http_fancyindex_entry_t;



/* 按名称降序比较目录条目 */
static int ngx_libc_cdecl
    ngx_http_fancyindex_cmp_entries_name_cs_desc(const void *one, const void *two);
/* 按大小降序比较目录条目 */
static int ngx_libc_cdecl
    ngx_http_fancyindex_cmp_entries_size_desc(const void *one, const void *two);
/* 按修改时间降序比较目录条目 */
static int ngx_libc_cdecl
    ngx_http_fancyindex_cmp_entries_mtime_desc(const void *one, const void *two);
/* 按名称升序比较目录条目 */
static int ngx_libc_cdecl
    ngx_http_fancyindex_cmp_entries_name_cs_asc(const void *one, const void *two);
/* 按大小升序比较目录条目 */
static int ngx_libc_cdecl
    ngx_http_fancyindex_cmp_entries_size_asc(const void *one, const void *two);
/* 按修改时间升序比较目录条目 */
static int ngx_libc_cdecl
    ngx_http_fancyindex_cmp_entries_mtime_asc(const void *one, const void *two);

/* 处理目录索引错误 */
static ngx_int_t ngx_http_fancyindex_error(ngx_http_request_t *r,
    ngx_dir_t *dir, ngx_str_t *name);

/* 模块初始化 */
static ngx_int_t ngx_http_fancyindex_init(ngx_conf_t *cf);

/* 创建位置配置 */
static void *ngx_http_fancyindex_create_loc_conf(ngx_conf_t *cf);

/* 合并位置配置 */
static char *ngx_http_fancyindex_merge_loc_conf(ngx_conf_t *cf,
    void *parent, void *child);

/* 设置忽略文件配置 */
static char *ngx_http_fancyindex_ignore(ngx_conf_t    *cf,
                                        ngx_command_t *cmd,
                                        void          *conf);

/* 转义文件名中的特殊字符 */
static uintptr_t
    ngx_fancyindex_escape_filename(u_char *dst, u_char*src, size_t size);

/*
 * 这些函数每个处理器调用只使用一次。我们可以告诉GCC尽可能始终内联它们
 * （请参阅上面ngx_force_inline的定义）。
 */
/* 创建页眉缓冲区 */
static ngx_inline ngx_buf_t*
    make_header_buf(ngx_http_request_t *r, const ngx_str_t css_href)
    ngx_force_inline;


/* 模块指令定义 */
static ngx_command_t  ngx_http_fancyindex_commands[] = {

    { ngx_string("fancyindex"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_FLAG,
      ngx_conf_set_flag_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_fancyindex_loc_conf_t, enable),
      NULL },

    { ngx_string("fancyindex_default_sort"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_enum_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_fancyindex_loc_conf_t, default_sort),
      &ngx_http_fancyindex_sort_criteria },
	
    { ngx_string("fancyindex_case_sensitive"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_FLAG,
      ngx_conf_set_flag_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_fancyindex_loc_conf_t, case_sensitive),
      NULL },

	
    { ngx_string("fancyindex_directories_first"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_FLAG,
      ngx_conf_set_flag_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_fancyindex_loc_conf_t, dirs_first),
      NULL },

    { ngx_string("fancyindex_localtime"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_FLAG,
      ngx_conf_set_flag_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_fancyindex_loc_conf_t, localtime),
      NULL },

    { ngx_string("fancyindex_exact_size"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_FLAG,
      ngx_conf_set_flag_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_fancyindex_loc_conf_t, exact_size),
      NULL },

    { ngx_string("fancyindex_header"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE12,
      ngx_fancyindex_conf_set_headerfooter,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_fancyindex_loc_conf_t, header),
      NULL },

    { ngx_string("fancyindex_footer"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE12,
      ngx_fancyindex_conf_set_headerfooter,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_fancyindex_loc_conf_t, footer),
      NULL },

    { ngx_string("fancyindex_css_href"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_FLAG,
      ngx_conf_set_str_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_fancyindex_loc_conf_t, css_href),
      NULL },

    { ngx_string("fancyindex_ignore"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_1MORE,
      ngx_http_fancyindex_ignore,
      NGX_HTTP_LOC_CONF_OFFSET,
      0,
      NULL },

    { ngx_string("fancyindex_hide_symlinks"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_FLAG,
      ngx_conf_set_flag_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_fancyindex_loc_conf_t, hide_symlinks),
      NULL },

    { ngx_string("fancyindex_show_path"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_FLAG,
      ngx_conf_set_flag_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_fancyindex_loc_conf_t, show_path),
      NULL },

    { ngx_string("fancyindex_show_dotfiles"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_FLAG,
      ngx_conf_set_flag_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_fancyindex_loc_conf_t, show_dot_files),
      NULL },

    { ngx_string("fancyindex_hide_parent_dir"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_FLAG,
      ngx_conf_set_flag_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_fancyindex_loc_conf_t, hide_parent),
      NULL },

    { ngx_string("fancyindex_time_format"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_FLAG,
      ngx_conf_set_str_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_fancyindex_loc_conf_t, time_format),
      NULL },

    ngx_null_command
};


static ngx_http_module_t  ngx_http_fancyindex_module_ctx = {
    NULL,                                  /* preconfiguration */
    ngx_http_fancyindex_init,              /* 配置后初始化 */

    NULL,                                  /* 创建主配置 */
    NULL,                                  /* 初始化主配置 */

    NULL,                                  /* 创建服务器配置 */
    NULL,                                  /* 合并服务器配置 */

    ngx_http_fancyindex_create_loc_conf,   /* 创建位置配置 */
    ngx_http_fancyindex_merge_loc_conf     /* 合并位置配置 */
};


ngx_module_t  ngx_http_fancyindex_module = {
    NGX_MODULE_V1,
    &ngx_http_fancyindex_module_ctx,       /* 模块上下文 */
    ngx_http_fancyindex_commands,          /* 模块指令 */
    NGX_HTTP_MODULE,                       /* 模块类型 */
    NULL,                                  /* 初始化master进程 */
    NULL,                                  /* 初始化模块 */
    NULL,                                  /* 初始化进程 */
    NULL,                                  /* 初始化线程 */
    NULL,                                  /* 退出线程 */
    NULL,                                  /* 退出进程 */
    NULL,                                  /* 退出master进程 */
    NGX_MODULE_V1_PADDING
};



static const ngx_str_t css_href_pre =
    ngx_string("<link rel=\"stylesheet\" href=\"");
static const ngx_str_t css_href_post =
    ngx_string("\" type=\"text/css\"/>\n");


#ifdef NGX_ESCAPE_URI_COMPONENT
static inline uintptr_t
ngx_fancyindex_escape_filename(u_char *dst, u_char *src, size_t size)
{
    return ngx_escape_uri(dst, src, size, NGX_ESCAPE_URI_COMPONENT);
}
#else /* !NGX_ESCAPE_URI_COMPONENT */
static uintptr_t
ngx_fancyindex_escape_filename(u_char *dst, u_char *src, size_t size)
{
    /*
     * ngx_escape_uri()函数不会转义冒号或问号(?)
     * 问号表示查询字符串的开始。因此我们需要自己处理这些字符。
     *
     * TODO: 一旦ngx_escape_uri()按预期工作，就移除这段代码！
     */

    u_int escapes = 0;
    u_char *psrc = src;
    size_t psize = size;

    while (psize--) {
        switch (*psrc++) {
            case ':':
            case '?':
            case '[':
            case ']':
                escapes++;
                break;
        }
    }

    if (dst == NULL) {
        return escapes + ngx_escape_uri(NULL, src, size, NGX_ESCAPE_HTML);
    }
    else if (escapes == 0) {
        /* 不需要额外转义，避免使用临时缓冲区 */
        return ngx_escape_uri(dst, src, size, NGX_ESCAPE_HTML);
    }
    else {
        uintptr_t uescapes = ngx_escape_uri(NULL, src, size, NGX_ESCAPE_HTML);
        size_t bufsz = size + 2 * uescapes;

        /*
     * GCC和CLANG都支持栈分配的变长
         * arrays. Take advantage of that to avoid a malloc-free cycle.
         */
#if defined(__GNUC__) || defined(__clang__)
        u_char cbuf[bufsz];
        u_char *buf = cbuf;
#else  /* __GNUC__ || __clang__ */
        u_char *buf = (u_char*) malloc(sizeof(u_char) * bufsz);
#endif /* __GNUC__ || __clang__ */

        ngx_escape_uri(buf, src, size, NGX_ESCAPE_HTML);

        while (bufsz--) {
            switch (*buf) {
                case ':':
                    *dst++ = '%';
                    *dst++ = '3';
                    *dst++ = 'A';
                    break;
                case '?':
                    *dst++ = '%';
                    *dst++ = '3';
                    *dst++ = 'F';
                    break;
                case '[':
                    *dst++ = '%';
                    *dst++ = '5';
                    *dst++ = 'B';
                    break;
                case ']':
                    *dst++ = '%';
                    *dst++ = '5';
                    *dst++ = 'D';
                    break;
                default:
                    *dst++ = *buf;
            }
            buf++;
        }

#if !defined(__GNUC__) && !defined(__clang__)
        free(buf);
#endif /* !__GNUC__ && !__clang__ */

        return escapes + uescapes;
    }
}
#endif /* NGX_ESCAPE_URI_COMPONENT */


/* 创建HTTP响应的头部缓冲区 */
static ngx_inline ngx_buf_t*
make_header_buf(ngx_http_request_t *r, const ngx_str_t css_href)
{
    ngx_buf_t *b;
    size_t blen = r->uri.len
        + ngx_sizeof_ssz(t01_head1)
        + ngx_sizeof_ssz(t02_head2)
        + ngx_sizeof_ssz(t03_head3)
        + ngx_sizeof_ssz(t04_body1)
        ;

    if (css_href.len) {
        blen += css_href_pre.len \
              + css_href.len \
              + css_href_post.len
              ;
    }

    if ((b = ngx_create_temp_buf(r->pool, blen)) == NULL)
        return NULL;

    b->last = ngx_cpymem_ssz(b->last, t01_head1);

    if (css_href.len) {
        b->last = ngx_cpymem_str(b->last, css_href_pre);
        b->last = ngx_cpymem_str(b->last, css_href);
        b->last = ngx_cpymem_str(b->last, css_href_post);
    }

    b->last = ngx_cpymem_ssz(b->last, t02_head2);
    b->last = ngx_cpymem_str(b->last, r->uri);
    b->last = ngx_cpymem_ssz(b->last, t03_head3);
    b->last = ngx_cpymem_ssz(b->last, t04_body1);

    return b;
}


/* 创建HTTP响应的内容缓冲区 */
static ngx_inline ngx_int_t
make_content_buf(
        ngx_http_request_t *r, ngx_buf_t **pb,
        ngx_http_fancyindex_loc_conf_t *alcf)
{
    ngx_http_fancyindex_entry_t *entry;

    int (*sort_cmp_func)(const void *, const void *);
    const char  *sort_url_args = "";

    off_t        length;
    size_t       len, root, allocated, escape_html;
    int64_t      multiplier;
    u_char      *filename, *last;
    ngx_tm_t     tm;
    ngx_array_t  entries;
    ngx_time_t  *tp;
    ngx_uint_t   i, j;
    ngx_str_t    path;
    ngx_dir_t    dir;
    ngx_buf_t   *b;

    static const char    *sizes[]  = { "EiB", "PiB", "TiB", "GiB", "MiB", "KiB", "B" };
    static const int64_t  exbibyte = 1024LL * 1024LL * 1024LL *
                                     1024LL * 1024LL * 1024LL;

    /*
     * NGX_DIR_MASK_LEN 小于 NGX_HTTP_FANCYINDEX_PREALLOCATE
     */
    if ((last = ngx_http_map_uri_to_path(r, &path, &root,
                    NGX_HTTP_FANCYINDEX_PREALLOCATE)) == NULL)
        return NGX_HTTP_INTERNAL_SERVER_ERROR;

    allocated = path.len;
    path.len  = last - path.data;
    path.data[path.len] = '\0';

    ngx_log_debug1(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                   "http fancyindex: \"%s\"", path.data);

    if (ngx_open_dir(&path, &dir) == NGX_ERROR) {
        ngx_int_t rc, err = ngx_errno;
        ngx_uint_t level;

        /* 处理不同类型的目录打开错误 */
        if (err == NGX_ENOENT || err == NGX_ENOTDIR || err == NGX_ENAMETOOLONG) {
            level = NGX_LOG_ERR;
            rc = NGX_HTTP_NOT_FOUND;  /* 文件不存在或不是目录或名称太长 */
        } else if (err == NGX_EACCES) {
            level = NGX_LOG_ERR;
            rc = NGX_HTTP_FORBIDDEN;  /* 权限不足 */
        } else {
            level = NGX_LOG_CRIT;
            rc = NGX_HTTP_INTERNAL_SERVER_ERROR;  /* 其他内部错误 */
        }

        ngx_log_error(level, r->connection->log, err,
                ngx_open_dir_n " \"%s\" failed", path.data);

        return rc;
    }

#if (NGX_SUPPRESS_WARN)
    /* MSVC认为'entries'可能在未初始化的情况下被使用 */
    ngx_memzero(&entries, sizeof(ngx_array_t));
#endif /* NGX_SUPPRESS_WARN */


    if (ngx_array_init(&entries, r->pool, 40,
                sizeof(ngx_http_fancyindex_entry_t)) != NGX_OK)
        return ngx_http_fancyindex_error(r, &dir, &path);

    filename = path.data;
    filename[path.len] = '/';

    /* 读取目录条目及其相关信息。 */
    for (;;) {
        ngx_set_errno(0);

        if (ngx_read_dir(&dir) == NGX_ERROR) {
            ngx_int_t err = ngx_errno;

            /* 如果不是因为没有更多文件而失败，则记录错误并返回 */
            if (err != NGX_ENOMOREFILES) {
                ngx_log_error(NGX_LOG_CRIT, r->connection->log, err,
                        ngx_read_dir_n " \"%V\" failed", &path);
                return ngx_http_fancyindex_error(r, &dir, &path);
            }
            break;
        }

        ngx_log_debug1(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                       "http fancyindex file: \"%s\"", ngx_de_name(&dir));

        len = ngx_de_namelen(&dir);

        if (!alcf->show_dot_files && ngx_de_name(&dir)[0] == '.')
            continue;

        if (alcf->hide_symlinks && ngx_de_is_link (&dir))
            continue;

#if NGX_PCRE
        /* 使用PCRE正则表达式匹配忽略文件 */
        {
            ngx_str_t str;
            str.len = len;
            str.data = ngx_de_name(&dir);

            if (alcf->ignore && ngx_regex_exec_array(alcf->ignore, &str,
                                                     r->connection->log)
                != NGX_DECLINED)
            {
                continue;  /* 匹配到忽略模式，跳过当前文件 */
            }
        }
#else /* !NGX_PCRE */
        /* 不使用PCRE，进行简单字符串匹配 */
        if (alcf->ignore) {
            u_int match_found = 0;
            ngx_str_t *s = alcf->ignore->elts;

            for (i = 0; i < alcf->ignore->nelts; i++, s++) {
                if (ngx_strcmp(ngx_de_name(&dir), s->data) == 0) {
                    match_found = 1;
                    break;
                }
            }

            if (match_found) {
                continue;
            }
        }
#endif /* NGX_PCRE */

        /* 目录条目信息无效，需要获取详细信息 */
        if (!dir.valid_info) {
            /* 1字节用于'/'，1字节用于终止符'\0' */
            if (path.len + 1 + len + 1 > allocated) {
                allocated = path.len + 1 + len + 1
                          + NGX_HTTP_FANCYINDEX_PREALLOCATE;

                if ((filename = ngx_palloc(r->pool, allocated)) == NULL)
                    return ngx_http_fancyindex_error(r, &dir, &path);

                last = ngx_cpystrn(filename, path.data, path.len + 1);
                *last++ = '/';
            }

            ngx_cpystrn(last, ngx_de_name(&dir), len + 1);

            /* 获取文件信息 */
            if (ngx_de_info(filename, &dir) == NGX_FILE_ERROR) {
                ngx_int_t err = ngx_errno;

                /* 如果不是文件不存在的错误，则记录并跳过 */
                if (err != NGX_ENOENT) {
                    ngx_log_error(NGX_LOG_ERR, r->connection->log, err,
                            ngx_de_info_n " \"%s\" failed", filename);
                    continue;
                }

                /* 尝试获取链接信息 */
                if (ngx_de_link_info(filename, &dir) == NGX_FILE_ERROR) {
                    ngx_log_error(NGX_LOG_CRIT, r->connection->log, ngx_errno,
                            ngx_de_link_info_n " \"%s\" failed", filename);
                    return ngx_http_fancyindex_error(r, &dir, &path);
                }
            }
        }

        if ((entry = ngx_array_push(&entries)) == NULL)
            return ngx_http_fancyindex_error(r, &dir, &path);

        entry->name.len  = len;
        entry->name.data = ngx_palloc(r->pool, len + 1);
        if (entry->name.data == NULL)
            return ngx_http_fancyindex_error(r, &dir, &path);

        ngx_cpystrn(entry->name.data, ngx_de_name(&dir), len + 1);
        entry->escape = 2 * ngx_fancyindex_escape_filename(NULL,
                                                           ngx_de_name(&dir),
                                                           len);
        entry->escape_html = ngx_escape_html(NULL,
                                             entry->name.data,
                                             entry->name.len);

        entry->dir     = ngx_de_is_dir(&dir);
        entry->mtime   = ngx_de_mtime(&dir);
        entry->size    = ngx_de_size(&dir);
        entry->utf_len = (r->headers_out.charset.len == 5 &&
                ngx_strncasecmp(r->headers_out.charset.data, (u_char*) "utf-8", 5) == 0)
            ?  ngx_utf8_length(entry->name.data, entry->name.len)
            : len;
    }

    if (ngx_close_dir(&dir) == NGX_ERROR) {
        ngx_log_error(NGX_LOG_ALERT, r->connection->log, ngx_errno,
                ngx_close_dir_n " \"%s\" failed", &path);
    }

    /*
     * 计算生成目录列表所需的缓冲区长度。
     * 包括URI、HTML标签、文件名、修改时间等内容。
     */

    escape_html = ngx_escape_html(NULL, r->uri.data, r->uri.len);

    if (alcf->show_path)
        len = r->uri.len + escape_html
          + ngx_sizeof_ssz(t05_body2)
          + ngx_sizeof_ssz(t06_list1)
          + ngx_sizeof_ssz(t_parentdir_entry)
          + ngx_sizeof_ssz(t07_list2)
          + ngx_fancyindex_timefmt_calc_size (&alcf->time_format) * entries.nelts
          ;
   else
        len = r->uri.len + escape_html
          + ngx_sizeof_ssz(t06_list1)
          + ngx_sizeof_ssz(t_parentdir_entry)
          + ngx_sizeof_ssz(t07_list2)
          + ngx_fancyindex_timefmt_calc_size (&alcf->time_format) * entries.nelts
          ;

    /*
     * 如果位于Web服务器根目录（URI = "/" --> 长度为1），
     * 不显示"上级目录"链接。
     */
    if (r->uri.len == 1) {
        len -= ngx_sizeof_ssz(t_parentdir_entry);
    }

    entry = entries.elts;
    for (i = 0; i < entries.nelts; i++) {
        /*
         * 生成的表格行如下所示，多余的空白已被去除：
         *
         *   <tr>
         *     <td><a href="U[?sort]">文件名</a></td>
         *     <td>大小</td><td>日期</td>
         *   </tr>
         */
       len += ngx_sizeof_ssz("<tr><td colspan=\"2\" class=\"link\"><a href=\"")
            + entry[i].name.len + entry[i].escape /* Escaped URL */
            + ngx_sizeof_ssz("?C=x&amp;O=y") /* URL排序参数 */
            + ngx_sizeof_ssz("\" title=\"")
            + entry[i].name.len + entry[i].utf_len + entry[i].escape_html
            + ngx_sizeof_ssz("\">")
            + entry[i].name.len + entry[i].utf_len + entry[i].escape_html
            + alcf->name_length + ngx_sizeof_ssz("&gt;")
            + ngx_sizeof_ssz("</a></td><td class=\"size\">")
            + 20 /* 文件大小 */
            + ngx_sizeof_ssz("</td><td class=\"date\">")    /* 日期前缀 */
            + ngx_sizeof_ssz("</td></tr>\n") /* 日期后缀 */
            + 2 /* 回车换行 */
            ;
    }

    if ((b = ngx_create_temp_buf(r->pool, len)) == NULL)
        return NGX_HTTP_INTERNAL_SERVER_ERROR;

    /*
     * 确定排序标准。URL参数格式如下：
     *
     *    C=x[&O=y]
     *
     * 其中x={M,S,N}表示排序依据(M:修改时间,S:大小,N:名称)，
     * y={A,D}表示排序方向(A:升序,D:降序)
     */
    if ((r->args.len == 3 || (r->args.len == 7 && r->args.data[3] == '&')) &&
        r->args.data[0] == 'C' && r->args.data[1] == '=')
    {
        /* 确定排序方向 */
        ngx_int_t sort_descending = r->args.len == 7
                                 && r->args.data[4] == 'O'
                                 && r->args.data[5] == '='
                                 && r->args.data[6] == 'D';

        /* 选择排序标准 */
        switch (r->args.data[2]) {
            case 'M': /* 按修改时间排序 */
                if (sort_descending) {
                    sort_cmp_func = ngx_http_fancyindex_cmp_entries_mtime_desc;
                    if (alcf->default_sort != NGX_HTTP_FANCYINDEX_SORT_CRITERION_DATE_DESC)
                        sort_url_args = "?C=M&amp;O=D";
                }
                else {
                    sort_cmp_func = ngx_http_fancyindex_cmp_entries_mtime_asc;
                    if (alcf->default_sort != NGX_HTTP_FANCYINDEX_SORT_CRITERION_DATE)
                        sort_url_args = "?C=M&amp;O=A";
                }
                break;
            case 'S': /* 按大小排序 */
                if (sort_descending) {
                    sort_cmp_func = ngx_http_fancyindex_cmp_entries_size_desc;
                    if (alcf->default_sort != NGX_HTTP_FANCYINDEX_SORT_CRITERION_SIZE_DESC)
                        sort_url_args = "?C=S&amp;O=D";
                }
                else {
                    sort_cmp_func = ngx_http_fancyindex_cmp_entries_size_asc;
                        if (alcf->default_sort != NGX_HTTP_FANCYINDEX_SORT_CRITERION_SIZE)
                    sort_url_args = "?C=S&amp;O=A";
                }
                break;
            case 'N': /* 按名称排序 */
            default:
                if (sort_descending) {
		sort_cmp_func = alcf->case_sensitive
                        ? ngx_http_fancyindex_cmp_entries_name_cs_desc
                        : ngx_http_fancyindex_cmp_entries_name_ci_desc;
                    if (alcf->default_sort != NGX_HTTP_FANCYINDEX_SORT_CRITERION_NAME_DESC)
                        sort_url_args = "?C=N&amp;O=D";
                }
                else {
                sort_cmp_func = alcf->case_sensitive
                        ? ngx_http_fancyindex_cmp_entries_name_cs_asc
                        : ngx_http_fancyindex_cmp_entries_name_ci_asc;
                    if (alcf->default_sort != NGX_HTTP_FANCYINDEX_SORT_CRITERION_NAME)
                        sort_url_args = "?C=N&amp;O=A";
                }
                break;
        }
    }
    else {
        switch (alcf->default_sort) {
            case NGX_HTTP_FANCYINDEX_SORT_CRITERION_DATE_DESC:
                sort_cmp_func = ngx_http_fancyindex_cmp_entries_mtime_desc;
                break;
            case NGX_HTTP_FANCYINDEX_SORT_CRITERION_DATE:
                sort_cmp_func = ngx_http_fancyindex_cmp_entries_mtime_asc;
                break;
            case NGX_HTTP_FANCYINDEX_SORT_CRITERION_SIZE_DESC:
                sort_cmp_func = ngx_http_fancyindex_cmp_entries_size_desc;
                break;
            case NGX_HTTP_FANCYINDEX_SORT_CRITERION_SIZE:
                sort_cmp_func = ngx_http_fancyindex_cmp_entries_size_asc;
                break;
            case NGX_HTTP_FANCYINDEX_SORT_CRITERION_NAME_DESC:
                sort_cmp_func = alcf->case_sensitive
                    ? ngx_http_fancyindex_cmp_entries_name_cs_desc
                    : ngx_http_fancyindex_cmp_entries_name_ci_desc;
                break;
            case NGX_HTTP_FANCYINDEX_SORT_CRITERION_NAME:
            default:
                sort_cmp_func = alcf->case_sensitive
                    ? ngx_http_fancyindex_cmp_entries_name_cs_asc
                    : ngx_http_fancyindex_cmp_entries_name_ci_asc;
                break;
        }
    }

    /* 如有需要，对条目进行排序 */
    if (entries.nelts > 1) {
        if (alcf->dirs_first)
        {
            ngx_http_fancyindex_entry_t *l, *r;

            l = entry;
            r = entry + entries.nelts - 1;
            while (l < r)
            {
                while (l < r && l->dir)
                    l++;
                while (l < r && !r->dir)
                    r--;
                if (l < r) {
                    /* 现在l指向文件而r指向目录 */
                    ngx_http_fancyindex_entry_t tmp;
                    tmp = *l;
                    *l = *r;
                    *r = tmp;
                }
            }
            if (r->dir)
                r++;

            if (r > entry)
                /* 对目录进行排序 */
                ngx_qsort(entry, (size_t)(r - entry),
                        sizeof(ngx_http_fancyindex_entry_t), sort_cmp_func);
            if (r < entry + entries.nelts)
                /* 对文件进行排序 */
                ngx_qsort(r, (size_t)(entry + entries.nelts - r),
                        sizeof(ngx_http_fancyindex_entry_t), sort_cmp_func);
        } else {
            ngx_qsort(entry, (size_t)entries.nelts,
                    sizeof(ngx_http_fancyindex_entry_t), sort_cmp_func);
        }
    }

    /* 如有需要，显示路径 */
    if (alcf->show_path){
        b->last = last = (u_char *) ngx_escape_html(b->last, r->uri.data, r->uri.len);
        b->last = ngx_cpymem_ssz(b->last, t05_body2);
    }

    /* 打开<table>标签 */
    b->last = ngx_cpymem_ssz(b->last, t06_list1);

    tp = ngx_timeofday();

    /* "上级目录"条目，如果显示则始终位于首位 */
    if (r->uri.len > 1 && alcf->hide_parent == 0) {
        b->last = ngx_cpymem_ssz(b->last,
                                 "<tr>"
                                 "<td colspan=\"2\" class=\"link\"><a href=\"../");
        if (*sort_url_args) {
            b->last = ngx_cpymem(b->last,
                                 sort_url_args,
                                 ngx_sizeof_ssz("?C=N&amp;O=A"));
        }
        b->last = ngx_cpymem_ssz(b->last,
                                 "\">上级目录/</a></td>"
                                 "<td class=\"size\">-</td>"
                                 "<td class=\"date\">-</td>"
                                 "</tr>"
                                 CRLF);
    }

    /* 目录和文件条目 */
    for (i = 0; i < entries.nelts; i++) {
        b->last = ngx_cpymem_ssz(b->last, "<tr><td colspan=\"2\" class=\"link\"><a href=\"");

        if (entry[i].escape) {
            ngx_fancyindex_escape_filename(b->last,
                                           entry[i].name.data,
                                           entry[i].name.len);

            b->last += entry[i].name.len + entry[i].escape;

        } else {
            b->last = ngx_cpymem_str(b->last, entry[i].name);
        }

        if (entry[i].dir) {
            *b->last++ = '/';
            if (*sort_url_args) {
                b->last = ngx_cpymem(b->last,
                                     sort_url_args,
                                     ngx_sizeof_ssz("?C=x&amp;O=y"));
            }
        }

        *b->last++ = '"';
        b->last = ngx_cpymem_ssz(b->last, " title=\"");
        b->last = (u_char *) ngx_escape_html(b->last, entry[i].name.data, entry[i].name.len);
        *b->last++ = '"';
        *b->last++ = '>';

        len = entry[i].utf_len;

        b->last = (u_char *) ngx_escape_html(b->last, entry[i].name.data, entry[i].name.len);
        last = b->last - 3;

	if (entry[i].dir) {
            *b->last++ = '/';
            len++;
        }
        b->last = ngx_cpymem_ssz(b->last, "</a></td><td class=\"size\">");
            last = b->last;
            b->last = ngx_utf8_cpystrn(b->last, entry[i].name.data,
                copy, entry[i].name.len);

            b->last = (u_char *) ngx_escape_html(last, entry[i].name.data, b->last - last);
            last = b->last;

        if (alcf->exact_size) {
            if (entry[i].dir) {
                *b->last++ = '-';
            } else {
                b->last = ngx_sprintf(b->last, "%19O", entry[i].size);
            }

        } else {
            if (entry[i].dir) {
                *b->last++ = '-';
            } else {
                length = entry[i].size;
                multiplier = exbibyte;

                for (j = 0; j < DIM(sizes) - 1 && length < multiplier; j++)
                    multiplier /= 1024;

                /* 如果以字节显示文件大小，则不显示小数 */
                if (j == DIM(sizes) - 1)
                    b->last = ngx_sprintf(b->last, "%O %s", length, sizes[j]);
                else
                    b->last = ngx_sprintf(b->last, "%.1f %s",
                                          (float) length / multiplier, sizes[j]);
            }
        }

        ngx_gmtime(entry[i].mtime + tp->gmtoff * 60 * alcf->localtime, &tm);
        b->last = ngx_cpymem_ssz(b->last, "</td><td class=\"date\">");
        b->last = ngx_fancyindex_timefmt(b->last, &alcf->time_format, &tm);
        b->last = ngx_cpymem_ssz(b->last, "</td></tr>");

        *b->last++ = CR;
        *b->last++ = LF;
    }

    /* 输出表格底部 */
    b->last = ngx_cpymem_ssz(b->last, t07_list2);

    *pb = b;
    return NGX_OK;
}



static ngx_int_t
ngx_http_fancyindex_handler(ngx_http_request_t *r)
{
    ngx_http_request_t             *sr;
    ngx_str_t                      *sr_uri;
    ngx_str_t                       rel_uri;
    ngx_int_t                       rc;
    ngx_http_fancyindex_loc_conf_t *alcf;
    ngx_chain_t                     out[3] = {
        { NULL, NULL }, { NULL, NULL}, { NULL, NULL }};


    if (r->uri.data[r->uri.len - 1] != '/') {
        return NGX_DECLINED;
    }

    /* TODO: Windows平台支持 */
#if defined(nginx_version) \
    && ((nginx_version < 7066) \
        || ((nginx_version > 8000) && (nginx_version < 8038)))
    /* 检查URI中是否包含空字符 */
    if (r->zero_in_uri) {
        return NGX_DECLINED;
    }
#endif

    /* 只处理GET和HEAD请求 */
    if (!(r->method & (NGX_HTTP_GET|NGX_HTTP_HEAD))) {
        return NGX_DECLINED;
    }

    alcf = ngx_http_get_module_loc_conf(r, ngx_http_fancyindex_module);

    /* 检查模块是否启用 */
    if (!alcf->enable) {
        return NGX_DECLINED;
    }

    if ((rc = make_content_buf(r, &out[0].buf, alcf)) != NGX_OK)
        return rc;

    out[0].buf->last_in_chain = 1;

    r->headers_out.status = NGX_HTTP_OK;
    r->headers_out.content_type_len  = ngx_sizeof_ssz("text/html");
    r->headers_out.content_type.len  = ngx_sizeof_ssz("text/html");
    r->headers_out.content_type.data = (u_char *) "text/html";

    rc = ngx_http_send_header(r);
    if (rc == NGX_ERROR || rc > NGX_OK || r->header_only)
        return rc;

    if (alcf->header.path.len > 0 && alcf->header.local.len == 0) {
        /* 已配置URI，让Nginx通过子请求处理。 */
        sr_uri = &alcf->header.path;

        if (*sr_uri->data != '/') {
            /* 相对路径 */
            rel_uri.len  = r->uri.len + alcf->header.path.len;
            rel_uri.data = ngx_palloc(r->pool, rel_uri.len);
            if (rel_uri.data == NULL) {
                return NGX_HTTP_INTERNAL_SERVER_ERROR;
            }
            ngx_memcpy(ngx_cpymem(rel_uri.data, r->uri.data, r->uri.len),
                    alcf->header.path.data, alcf->header.path.len);
            sr_uri = &rel_uri;
        }

        ngx_log_debug1(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                "http fancyindex: header subrequest \"%V\"", sr_uri);

        rc = ngx_http_subrequest(r, sr_uri, NULL, &sr, NULL, 0);
        if (rc == NGX_ERROR || rc == NGX_DONE) {
            ngx_log_debug1(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                    "http fancyindex: header subrequest for \"%V\" failed", sr_uri);
            return rc;
        }

        ngx_log_debug1(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                "http fancyindex: header subrequest status = %i",
                sr->headers_out.status);
        /* ngx_http_subrequest returns NGX_OK(0), not NGX_HTTP_OK(200) */
        if (sr->headers_out.status != NGX_OK) {
            /*
             * XXX: 如果我们收到404以外的状态码，是否应该在错误日志中记录消息？
             */
            goto add_builtin_header;
        }
    }
    else {
add_builtin_header:
        /* 在此之前创建空间 */
        out[1].next = out[0].next;
        out[1].buf  = out[0].buf;
        /* 链接页眉缓冲区 */
        out[0].next = &out[1];
        if (alcf->header.local.len > 0) {
            /* 页眉缓冲区为本地，创建指向数据的缓冲区。 */
            out[0].buf = ngx_calloc_buf(r->pool);
            if (out[0].buf == NULL)
                return NGX_ERROR;
            out[0].buf->memory = 1;
            out[0].buf->pos = alcf->header.local.data;
            out[0].buf->last = alcf->header.local.data + alcf->header.local.len;
        } else {
            /* 准备包含内置页眉内容的缓冲区。 */
            out[0].buf = make_header_buf(r, alcf->css_href);
        }
    }

    /* 如果页脚已禁用，链接页脚缓冲区。 */
    if (alcf->footer.path.len == 0 || alcf->footer.local.len > 0) {
        ngx_uint_t last = (alcf->header.path.len == 0) ? 2 : 1;

        out[last-1].next = &out[last];
        out[last].buf = ngx_calloc_buf(r->pool);
        if (out[last].buf == NULL)
            return NGX_ERROR;

        out[last].buf->memory = 1;
        if (alcf->footer.local.len > 0) {
            out[last].buf->pos = alcf->footer.local.data;
            out[last].buf->last = alcf->footer.local.data + alcf->footer.local.len;
        } else {
            out[last].buf->pos = (u_char*) t08_foot1;
            out[last].buf->last = (u_char*) t08_foot1 + sizeof(t08_foot1) - 1;
        }

        out[last-1].buf->last_in_chain = 0;
        out[last].buf->last_in_chain   = 1;
        out[last].buf->last_buf        = 1;
        /* 一次性发送所有内容 :D */
        return ngx_http_output_filter(r, &out[0]);
    }

    /*
     * 如果到达此处，说明需要发送自定义页脚。我们需要：
     * 部分发送out[0]引用的内容，然后将页脚作为子请求发送。
     * 如果子请求失败，我们也应发送标准页脚。
     */
    rc = ngx_http_output_filter(r, &out[0]);

    if (rc != NGX_OK && rc != NGX_AGAIN)
        return NGX_HTTP_INTERNAL_SERVER_ERROR;

    /* 已配置URI，让Nginx通过子请求处理。 */
    sr_uri = &alcf->footer.path;

    if (*sr_uri->data != '/') {
        /* 相对路径 */
        rel_uri.len  = r->uri.len + alcf->footer.path.len;
        rel_uri.data = ngx_palloc(r->pool, rel_uri.len);
        if (rel_uri.data == NULL) {
            return NGX_HTTP_INTERNAL_SERVER_ERROR;
        }
        ngx_memcpy(ngx_cpymem(rel_uri.data, r->uri.data, r->uri.len),
                alcf->footer.path.data, alcf->footer.path.len);
        sr_uri = &rel_uri;
    }

    ngx_log_debug1(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
            "http fancyindex: footer subrequest \"%V\"", sr_uri);

    rc = ngx_http_subrequest(r, sr_uri, NULL, &sr, NULL, 0);
    if (rc == NGX_ERROR || rc == NGX_DONE) {
        ngx_log_debug1(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                "http fancyindex: footer subrequest for \"%V\" failed", sr_uri);
        return rc;
    }

    ngx_log_debug1(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
            "http fancyindex: header subrequest status = %i",
            sr->headers_out.status);

    /* 参见上面：ngx_http_subrequest返回NGX_OK (0)而不是NGX_HTTP_OK (200) */
    if (sr->headers_out.status != NGX_OK) {
        /*
         * XXX: 如果我们收到404以外的状态码，是否应该在错误日志中记录消息？
         */
        out[0].next = NULL;
        out[0].buf = ngx_calloc_buf(r->pool);
        if (out[0].buf == NULL)
            return NGX_ERROR;
        out[0].buf->memory = 1;
        out[0].buf->pos = (u_char*) t08_foot1;
        out[0].buf->last = (u_char*) t08_foot1 + sizeof(t08_foot1) - 1;
        out[0].buf->last_in_chain = 1;
        out[0].buf->last_buf = 1;
        /* 直接发送内置页脚 */
        return ngx_http_output_filter(r, &out[0]);
    }

    return (r != r->main) ? rc : ngx_http_send_special(r, NGX_HTTP_LAST);
}


/* 按名称降序比较目录条目 */
static int ngx_libc_cdecl
ngx_http_fancyindex_cmp_entries_name_desc(const void *one, const void *two)
{
    ngx_http_fancyindex_entry_t *first = (ngx_http_fancyindex_entry_t *) one;
    ngx_http_fancyindex_entry_t *second = (ngx_http_fancyindex_entry_t *) two;

    return (int) ngx_strcmp(second->name.data, first->name.data);
}


/* 按大小降序比较目录条目 */
static int ngx_libc_cdecl
ngx_http_fancyindex_cmp_entries_size_desc(const void *one, const void *two)
{
    ngx_http_fancyindex_entry_t *first = (ngx_http_fancyindex_entry_t *) one;
    ngx_http_fancyindex_entry_t *second = (ngx_http_fancyindex_entry_t *) two;

    return (first->size < second->size) - (first->size > second->size);
}


/* 按修改时间降序比较目录条目 */
static int ngx_libc_cdecl
ngx_http_fancyindex_cmp_entries_mtime_desc(const void *one, const void *two)
{
    ngx_http_fancyindex_entry_t *first = (ngx_http_fancyindex_entry_t *) one;
    ngx_http_fancyindex_entry_t *second = (ngx_http_fancyindex_entry_t *) two;

    return (int) (second->mtime - first->mtime);
}


/* 按名称升序比较目录条目 */
static int ngx_libc_cdecl
ngx_http_fancyindex_cmp_entries_name_asc(const void *one, const void *two)
{
    ngx_http_fancyindex_entry_t *first = (ngx_http_fancyindex_entry_t *) one;
    ngx_http_fancyindex_entry_t *second = (ngx_http_fancyindex_entry_t *) two;

    return (int) ngx_strcmp(first->name.data, second->name.data);
}


/* 按大小升序比较目录条目 */
static int ngx_libc_cdecl
ngx_http_fancyindex_cmp_entries_size_asc(const void *one, const void *two)
{
    ngx_http_fancyindex_entry_t *first = (ngx_http_fancyindex_entry_t *) one;
    ngx_http_fancyindex_entry_t *second = (ngx_http_fancyindex_entry_t *) two;

    return (first->size > second->size) - (first->size < second->size);
}


/* 按修改时间升序比较目录条目 */
static int ngx_libc_cdecl
ngx_http_fancyindex_cmp_entries_mtime_asc(const void *one, const void *two)
{
    ngx_http_fancyindex_entry_t *first = (ngx_http_fancyindex_entry_t *) one;
    ngx_http_fancyindex_entry_t *second = (ngx_http_fancyindex_entry_t *) two;

    return (int) (first->mtime - second->mtime);
}


/* 处理目录索引错误 */
static ngx_int_t
ngx_http_fancyindex_error(ngx_http_request_t *r, ngx_dir_t *dir, ngx_str_t *name)
{
    if (ngx_close_dir(dir) == NGX_ERROR) {
        ngx_log_error(NGX_LOG_ALERT, r->connection->log, ngx_errno,
                      ngx_close_dir_n " \"%V\" failed", name);
    }

    return NGX_HTTP_INTERNAL_SERVER_ERROR;
}


/* 创建位置配置 */
static void *
ngx_http_fancyindex_create_loc_conf(ngx_conf_t *cf)
{
    ngx_http_fancyindex_loc_conf_t  *conf;

    conf = ngx_pcalloc(cf->pool, sizeof(ngx_http_fancyindex_loc_conf_t));
    if (conf == NULL) {
        return NGX_CONF_ERROR;
    }

    /*
     * 由ngx_pcalloc设置：
     *    conf->header.*.len     = 0
     *    conf->header.*.data    = NULL
     *    conf->footer.*.len     = 0
     *    conf->footer.*.data    = NULL
     *    conf->css_href.len     = 0
     *    conf->css_href.data    = NULL
     *    conf->time_format.len  = 0
     *    conf->time_format.data = NULL
     */
    conf->enable         = NGX_CONF_UNSET;
    conf->default_sort   = NGX_CONF_UNSET_UINT;
    conf->dirs_first     = NGX_CONF_UNSET;
    conf->localtime      = NGX_CONF_UNSET;
    conf->name_length    = NGX_CONF_UNSET_UINT;
    conf->exact_size     = NGX_CONF_UNSET;
    conf->ignore         = NGX_CONF_UNSET_PTR;
    conf->hide_symlinks  = NGX_CONF_UNSET;
    conf->show_path      = NGX_CONF_UNSET;
    conf->hide_parent    = NGX_CONF_UNSET;
    conf->show_dot_files = NGX_CONF_UNSET;

    return conf;
}


/* 合并位置配置 */
static char *
ngx_http_fancyindex_merge_loc_conf(ngx_conf_t *cf, void *parent, void *child)
{
    ngx_http_fancyindex_loc_conf_t *prev = parent;
    ngx_http_fancyindex_loc_conf_t *conf = child;

    (void) cf; /* 未使用 */

    ngx_conf_merge_value(conf->enable, prev->enable, 0);
    ngx_conf_merge_uint_value(conf->default_sort, prev->default_sort, NGX_HTTP_FANCYINDEX_SORT_CRITERION_NAME);
    ngx_conf_merge_value(conf->dirs_first, prev->dirs_first, 1);
    ngx_conf_merge_value(conf->localtime, prev->localtime, 0);
    ngx_conf_merge_value(conf->exact_size, prev->exact_size, 1);
    ngx_conf_merge_value(conf->show_path, prev->show_path, 1);
    ngx_conf_merge_value(conf->show_dot_files, prev->show_dot_files, 0);
    ngx_conf_merge_uint_value(conf->name_length, prev->name_length, 50);

    ngx_conf_merge_str_value(conf->header.path, prev->header.path, "");
    ngx_conf_merge_str_value(conf->header.path, prev->header.local, "");
    ngx_conf_merge_str_value(conf->footer.path, prev->footer.path, "");
    ngx_conf_merge_str_value(conf->footer.path, prev->footer.local, "");

    ngx_conf_merge_str_value(conf->css_href, prev->css_href, "");
    ngx_conf_merge_str_value(conf->time_format, prev->time_format, "%Y-%b-%d %H:%M");

    ngx_conf_merge_ptr_value(conf->ignore, prev->ignore, NULL);
    ngx_conf_merge_value(conf->hide_symlinks, prev->hide_symlinks, 0);
    ngx_conf_merge_value(conf->hide_parent, prev->hide_parent, 0);

    /* 确保在未提供自定义页眉的情况下没有禁用show_path指令 */
    if (conf->show_path == 0 && conf->header.path.len == 0)
    {
        ngx_conf_log_error(NGX_LOG_EMERG, cf, 0, "FancyIndex : cannot set show_path to off without providing a custom header !");
        return NGX_CONF_ERROR;
    }

    return NGX_CONF_OK;
}


/* 设置忽略文件配置 */
static char*
ngx_http_fancyindex_ignore(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
    ngx_http_fancyindex_loc_conf_t *alcf = conf;
    ngx_str_t *value;

    (void) cmd; /* 未使用 */

#if (NGX_PCRE)
    ngx_uint_t          i;
    ngx_regex_elt_t    *re;
    ngx_regex_compile_t rc;
    u_char              errstr[NGX_MAX_CONF_ERRSTR];

    if (alcf->ignore == NGX_CONF_UNSET_PTR) {
        alcf->ignore = ngx_array_create(cf->pool, 2, sizeof(ngx_regex_elt_t));
        if (alcf->ignore == NULL) {
            return NGX_CONF_ERROR;
        }
    }

    value = cf->args->elts;

    ngx_memzero(&rc, sizeof(ngx_regex_compile_t));

    rc.err.data = errstr;
    rc.err.len  = NGX_MAX_CONF_ERRSTR;
    rc.pool     = cf->pool;

    for (i = 1; i < cf->args->nelts; i++) {
        re = ngx_array_push(alcf->ignore);
        if (re == NULL) {
            return NGX_CONF_ERROR;
        }

        rc.pattern = value[i];
        rc.options = NGX_REGEX_CASELESS;

        if (ngx_regex_compile(&rc) != NGX_OK) {
            ngx_conf_log_error(NGX_LOG_EMERG, cf, 0, "%V", &rc.err);
            return NGX_CONF_ERROR;
        }

        re->name  = value[i].data;
        re->regex = rc.regex;
    }

    return NGX_CONF_OK;
#else /* !NGX_PCRE */
    ngx_uint_t i;
    ngx_str_t *str;

    if (alcf->ignore == NGX_CONF_UNSET_PTR) {
        alcf->ignore = ngx_array_create(cf->pool, 2, sizeof(ngx_str_t));
        if (alcf->ignore == NULL) {
            return NGX_CONF_ERROR;
        }
    }

    value = cf->args->elts;

    for (i = 1; i < cf->args->nelts; i++) {
        str = ngx_array_push(alcf->ignore);
        if (str == NULL) {
            return NGX_CONF_ERROR;
        }

        str->data = value[i].data;
        str->len  = value[i].len;
    }

    return NGX_CONF_OK;
#endif /* NGX_PCRE */

}


static ngx_int_t
ngx_http_fancyindex_init(ngx_conf_t *cf)
{
    ngx_http_handler_pt        *h;
    ngx_http_core_main_conf_t  *cmcf;

    cmcf = ngx_http_conf_get_module_main_conf(cf, ngx_http_core_module);

    h = ngx_array_push(&cmcf->phases[NGX_HTTP_CONTENT_PHASE].handlers);
    if (h == NULL) {
        return NGX_ERROR;
    }

    *h = ngx_http_fancyindex_handler;

    return NGX_OK;
}

/* vim:et:sw=4:ts=4:
 */
