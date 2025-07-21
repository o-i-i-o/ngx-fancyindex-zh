#! /usr/bin/awk -f
# 经过测试，win-awk将导致乱码。
# Copyright © Adrian Perez <aperez@igalia.com>
#
# 将HTML模板转换为适合包含的C头文件。
# 查看HACKING.rst文件了解使用方法 :-)
#
# 此代码已放入公共领域。

BEGIN {
	varname = 0;
	print "/* 此文件自动生成，请勿修改! */"
	vars_count = 0;
}

/^<!--[[:space:]]*var[[:space:]]+[^[:space:]]+[[:space:]]*-->$/ {
	if (varname) print ";";
	if ($3 == "NONE") {
		varname = 0;
		next;
	}
	varname = $3;
	vars[vars_count++] = varname;
	print "static const u_char " varname "[] = \"\"";
	next;
}

/^$/ {
	if (!varname) next;
	print "\"\\n\"";
	next;
}

{
	if (!varname) next;
	# Order matters
	gsub(/[\t\v\n\r\f]+/, "");
	gsub(/\\/, "\\\\");
	gsub(/"/, "\\\"");
	print "\"" $0 "\""
}


END {
	if (varname) print ";";
	print "#define NFI_TEMPLATE_SIZE (0 \\";
	for (var in vars) {
		print "\t+ nfi_sizeof_ssz(" vars[var] ") \\";
	}
	print "\t)"
}

