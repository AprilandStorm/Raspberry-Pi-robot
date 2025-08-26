#!/bin/bash
# 如果用户通过 -o 指定了文件名，就用用户的；否则自动生成时间戳名字

OUTPUT_FILE=""

# 解析参数，看用户有没有指定 -o
while [[ $# -gt 0 ]]; do
  case $1 in
    -o|--output)
      OUTPUT_FILE="$2"
      shift 2
      ;;
    *)
      shift
      ;;
  esac
done

# 如果没指定 -o，自动生成文件名
if [ -z "$OUTPUT_FILE" ]; then
  OUTPUT_FILE="$(date +%Y%m%d-%H%M%S).jpg"
fi

# 执行拍照命令
rpicam-still --vflip -o "$OUTPUT_FILE" "$@"
