echo "➤ lz4" && ./build_and_test.sh -lz4 -nobuild | grep OK
echo "➤ apultra" && ./build_and_test.sh -apultra -nobuild | grep OK
echo "➤ zx7b" && ./build_and_test.sh -zx7b -nobuild | grep OK
echo "➤ blz" && ./build_and_test.sh -blz -nobuild | grep OK
echo "➤ exo" && ./build_and_test.sh -exo -nobuild | grep OK
echo "➤ pp" && ./build_and_test.sh -pp -nobuild | grep OK
echo "➤ snappy" && ./build_and_test.sh -snappy -nobuild | grep OK 
echo "➤ doboz" && ./build_and_test.sh -doboz -nobuild | grep OK 
echo "➤ qlz" && ./build_and_test.sh -qlz -nobuild | grep OK 
echo "➤ lzav" && ./build_and_test.sh -lzav -nobuild | grep OK
echo "➤ lzma" && ./build_and_test.sh -lzma -nobuild | grep OK
echo "➤ zstd" && ./build_and_test.sh -zstd -nobuild | grep OK
echo "➤ shrinkler" && ./build_and_test.sh -shr -nobuild | grep OK
echo "➤ stcr" && ./build_and_test.sh -stcr -nobuild | grep OK 
echo "➤ lzsa2" && ./build_and_test.sh -lzsa -nobuild | grep OK 
echo "➤ zx0" && ./build_and_test.sh -zx0 -nobuild | grep OK

