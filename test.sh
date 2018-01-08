#!/bin/bash
program="file_system"
disc_name="disc_a"
size=1000000

text_file0="a.txt"
text_file1="b.txt"
img0="tree.jpg"
img1="sky.jpg"

gcc fs.c -o $program

echo "----- TEST --------"
./$program $disc_name -n $size

echo "-- ADD FILES TO DISC --"
./$program $disc_name -v $text_file0
./$program $disc_name -v $img0

echo "-- LIST DISC DIR --"
./$program $disc_name -l

echo "-- DELETE $text_file0 --"
./$program $disc_name -d $text_file0

echo "-- LIST DISC DIR --"
./$program $disc_name -l

echo "-- ADD FILES TO DISC --"
./$program $disc_name -v $text_file1
./$program $disc_name -v $img1
./$program $disc_name -v $text_file0

echo "-- LIST DISC DIR --"
./$program $disc_name -l

mkdir tmp
mv $text_file0 tmp/
mv $text_file1 tmp/
mv $img0 tmp/
mv $img1 tmp/

echo "COPY FILES FROM DISC TO DIR"
./$program $disc_name -c $text_file0
./$program $disc_name -c $text_file1
./$program $disc_name -c $img0
./$program $disc_name -c $img1

echo "COMPARE COPIED FILES TO ORIGINAL ONES"
diff $text_file0 tmp/$text_file0
diff $text_file1 tmp/$text_file1
diff $img0 tmp/$img0
diff $img1 tmp/$img1
