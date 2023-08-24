#!/bin/sh
output_path=output.iconset
input_file=IconBase_Layers_1024x1024.png

mkdir $output_path

# the convert command comes from imagemagick
for size in 16 32 64 128 256; do
  half="$(($size / 2))"
  convert $input_file -resize x$size $output_path/icon_${size}x${size}.png
  convert $input_file -resize x$size $output_path/icon_${half}x${half}@2x.png
done

iconutil -c icns $output_path

cp output.icns ../CMake/GoatEdit.icns
