#!/bin/sh -e

export PATH=/sbin:/bin:/usr/sbin:/usr/bin:/usr/local/bin
ROOT=$1

test ! -z "$ROOT" || ROOT=$PWD

install_extra() {
        local line=$1
        local target_path="`dirname $line`"
        test -d "$ROOT$target_path" || mkdir -p $ROOT$target_path
	local link="`readlink "$line" || true`"
	if echo "$link" | grep -q "^\/"; then
		install_extra "$link"
		ln -srf $ROOT$link $ROOT$line
		echo ln -srf $ROOT$link $ROOT$line
	elif test -z "$link"; then
		if test -d "$ROOT$line" || test -f "$ROOT$line"; then
			return
		fi
		if test -d "$line"; then
			ls $line | while read file; do
				install_extra "$line/$file"
			done
		else
			cp -r $line $ROOT/$line
			echo cp -r $line $ROOT$line
		fi
	else
		install_extra "$target_path/$link"
		ln -srf $ROOT$target_path/$link $ROOT$line
		echo ln -srf $ROOT$target_path/$link $ROOT$line 
	fi
}                                                                                                        

install_bin() {
        local path=$1
	if ! echo "$path" | grep -q "^\/"; then
		path="`which $path`"
	fi
	test -f $ROOT$path && return 0 || true
	echo install_bin $path >&2
        ldd $path | tail -n +1 | while read filename arrow ipath rest; do
		if test -z "$ipath"; then
			echo "$filename" | grep -q "^\/" \
				&& install_bin "$filename" \
				|| true
		       	continue
		fi
                install_bin "$ipath"
        done
	install_extra $path
}

cmount() {
	local type=$1
	shift
	if ! mount | grep -q "on $ROOT/$type type"; then
		mkdir -p $ROOT/$type 2>/dev/null || true
		mount $@ $ROOT/$type
	fi
}

cmknod() {
	local mode=$1
	local type=$2
	local major=$3
	local minor=$4
	local path=$5
	test -$2 $ROOT/$path && return 0 || true
	mknod $ROOT/$path $type $major $minor $path
	chmod $mode $ROOT/$path
}

cmount dev --bind /dev
cmount sys --bind /sys
cmount proc --bind /proc
cmknod 666 c 5 2 dev/ptmx
cmount dev/pts -t devpts devpts
cat .chroot | while read line; do install_bin "$line"; done
cat .chroot-extra | while read line; do install_extra "$line"; done

mkdir chroot_node_modules 2>/dev/null || true
cd node_modules
ls | while read path; do
	link="`readlink $path`"
	if test -z "$link"; then
		ls $path | while read next; do echo $path/$next; done
	else
		echo $path
	fi
done | while read path; do
	echo cp -r `readlink -f $path` ../chroot_node_modules/$path >&2
	mkdir -p `dirname ../chroot_node_modules/$path` || true
	cp -r `readlink -f $path` ../chroot_node_modules/$path
done
cd - >/dev/null
