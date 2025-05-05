#!/bin/bash

args=("sponza" "japanese" "warehouse2")

vname="ref"

samples=64000
bounces=16

width=1024
height=1024
format=png

for scene in "${args[@]}"; do
    echo "Running with scene: $scene"
    input="data/scene/$scene.scn"
    output="data/result/$scene/$vname.$format"
    bin/pt $input --headless -o $output -w $width -h $height -f $format -b $bounces -s $samples
done
