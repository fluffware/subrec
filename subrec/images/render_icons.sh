
render() # svg_file size
{
    local svg_file=$1
    local size=$2
    local png_dir="hicolor/${size}x${size}"
    if test ! -e "$pngdir" ; then mkdir "$png_dir"; fi
    local png_file=`basename $svg_file .svg`.png
    inkscape "$svg_file" --export-png="$png_dir/$png_file" --export-area-page  --export-width=$size --export-height=$size
}

render_all_sizes() # svg_file
{
    local svg_file=$1
    for s in 22 32 48 ; do
	render $svg_file $s
    done
}

render_all_sizes subrec.svg
