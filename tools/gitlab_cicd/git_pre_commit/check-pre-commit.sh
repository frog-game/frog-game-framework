#!/bin/sh

if git rev-parse --verify HEAD >/dev/null 2>&1
then
	against=HEAD
else
	against=4b825dc642cb6eb9a060e54bf8d69288fbee4904
fi

astyle_lint_dir="./tools/gitlab_cicd/format_check/win"

changed_c_cpp_files=$(git diff-index --cached $against | \
	 grep -E '[MA]	.*\.(c|cpp|h)$' | \
	 grep -v 'glog' | \
	 cut -d'	' -f 2)
	
changed_lua_files=$(git diff-index --cached $against | \
	grep -E '[MA]	.*\.(lua)$' | \
	grep -v 'glog' | \
	cut -d'	' -f 2)
	

#c,c++代码风格检测
c_cpp_int_ret=0
if [ -n "$changed_c_cpp_files" ]; then
	python $astyle_lint_dir/../run-clang-format.py --clang-format-executable $astyle_lint_dir/clang-format.exe -r $changed_c_cpp_files 
	c_cpp_int_ret=$?
fi

if [ "$c_cpp_int_ret" != 0 ]; then
	$astyle_lint_dir/clang-format.exe -style=file -i $changed_c_cpp_files
	echo -e "[c,c++提交失败!!!]\n上传的代码中存在一些不规范的地方, 已经自动对相关代码按照规范要求做了格式化操作, 请重新提交!\n若仍然存在不规范的地方, 请手动修改并提交, 直到所有代码都符合规范为止...\n\n"
fi

#lua代码风格检测
lua_int_ret=0
if [ -n "$changed_lua_files" ]; then
	for file in $changed_lua_files;
	do
		$astyle_lint_dir/CodeFormat.exe check -f $file -DAE -c .editorconfig 
		lua_int_ret=$?
		if [ "$lua_int_ret" != 0 ]; then
			echo "格式化不规范的lua文件:${file}" 
		fi
	done
fi

if [ "$lua_int_ret" != 0 ]; then
	for file in $changed_lua_files;
	do
		$astyle_lint_dir/CodeFormat.exe format -f $file -o $file -c .editorconfig
	done
	
	echo -e "[lua代码提交失败!!!]\n上传的代码中存在一些不规范的地方, 已经自动对相关代码按照规范要求做了格式化操作, 请重新提交!\n若仍然存在不规范的地方, 请手动修改并提交, 直到所有代码都符合规范为止..."
fi

# lua代码语法诊断检测
lua_diagnosis_ret=0
if [ -n "$changed_lua_files" ]; then
	$astyle_lint_dir/luacheck.exe $changed_lua_files --no-config --no-default-config --codes -q --exclude-files **/config.lua **/middleclass.lua --ignore 311
	lua_diagnosis_ret=$?
fi

if [ "$lua_diagnosis_ret" != 0 ]; then
	echo -e "[lua代码提交失败!!!]\n上传的代码中存在一些语法不规范的地方, 请手动修改并提交, 直到所有代码都符合规范为止..."
fi

if [[ "$c_cpp_int_ret" != 0 || "$lua_int_ret" != 0 || "$lua_diagnosis_ret" != 0 ]]; then
	exit 1
fi 