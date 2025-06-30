#! /usr/bin/env bash

SOURCE=$1
TARGET=$2

mkdir -p ${TARGET}
TARGET=$(realpath ${TARGET})

if [ -z "${SOURCE}" ]; then
    echo "Source folder is not set!"
    exit 1
fi

if [ -z "${TARGET}" ]; then
    echo "Target folder is not set!"
    exit 1
fi

function copyfile() {
    local relative=$1
    local dir=${TARGET}/$(dirname $relative)
    mkdir -p ${dir}

    cp -f -v ${SOURCE}/${relative} ${TARGET}/${relative}
}

function copyfile2() {
    local relative=$1
    local tgt=${TARGET}/${relative}
    if [ -e $1 ]; then
        mkdir -p $(dirname $tgt)
        cp -f -v $1 ${tgt}
    fi
}

# This is copied from the rive-cpp/dependencies/premake5_harfbuzz_v2.lua
# (version harfbuzz 11.2.1)

echo "// generated do not edit" > ${TARGET}/config.h
echo "#define RELEASE" >> ${TARGET}/config.h
echo "#define NDEBUG" >> ${TARGET}/config.h
echo "#define HAVE_OT" >> ${TARGET}/config.h
echo "#define HB_NO_FALLBACK_SHAPE" >> ${TARGET}/config.h
echo "#define HB_NO_WIN1256" >> ${TARGET}/config.h
echo "#define HB_NO_EXTERN_HELPERS" >> ${TARGET}/config.h
echo "#define HB_DISABLE_DEPRECATED" >> ${TARGET}/config.h
echo "#define HB_NO_COLOR" >> ${TARGET}/config.h
echo "#define HB_NO_BITMAP" >> ${TARGET}/config.h
echo "#define HB_NO_BUFFER_SERIALIZE" >> ${TARGET}/config.h
echo "#define HB_NO_SETLOCALE" >> ${TARGET}/config.h
echo "#define HB_NO_VERTICAL" >> ${TARGET}/config.h
echo "#define HB_NO_LAYOUT_COLLECT_GLYPHS" >> ${TARGET}/config.h
echo "#define HB_NO_LAYOUT_RARELY_USED" >> ${TARGET}/config.h
echo "#define HB_NO_LAYOUT_UNUSED" >> ${TARGET}/config.h
echo "#define HB_NO_OT_FONT_GLYPH_NAMES" >> ${TARGET}/config.h

pushd ${SOURCE}

for f in *.{h,hh} ; do
    copyfile2 $f
done

for f in graph/*.{h,hh} ; do
    copyfile2 $f
done

cp -r -v ./OT ${TARGET}

popd


copyfile 'hb-aat-layout.cc'
copyfile 'hb-aat-map.cc'
copyfile 'hb-blob.cc'
copyfile 'hb-buffer-serialize.cc'
copyfile 'hb-buffer-verify.cc'
copyfile 'hb-buffer.cc'
copyfile 'hb-common.cc'
copyfile 'hb-draw.cc'
copyfile 'hb-face.cc'
copyfile 'hb-face-builder.cc'
copyfile 'hb-font.cc'
copyfile 'hb-map.cc'
copyfile 'hb-number.cc'
copyfile 'hb-ot-cff1-table.cc'
copyfile 'hb-ot-cff2-table.cc'
copyfile 'hb-ot-color.cc'
copyfile 'hb-ot-face.cc'
copyfile 'hb-ot-font.cc'
copyfile 'hb-ot-layout.cc'
copyfile 'hb-ot-map.cc'
copyfile 'hb-ot-math.cc'
copyfile 'hb-ot-meta.cc'
copyfile 'hb-ot-metrics.cc'
copyfile 'hb-ot-name.cc'
copyfile 'hb-ot-shaper-arabic.cc'
copyfile 'hb-ot-shaper-default.cc'
copyfile 'hb-ot-shaper-hangul.cc'
copyfile 'hb-ot-shaper-hebrew.cc'
copyfile 'hb-ot-shaper-indic-table.cc'
copyfile 'hb-ot-shaper-indic.cc'
copyfile 'hb-ot-shaper-khmer.cc'
copyfile 'hb-ot-shaper-myanmar.cc'
copyfile 'hb-ot-shaper-syllabic.cc'
copyfile 'hb-ot-shaper-thai.cc'
copyfile 'hb-ot-shaper-use.cc'
copyfile 'hb-ot-shaper-vowel-constraints.cc'
copyfile 'hb-ot-shape-fallback.cc'
copyfile 'hb-ot-shape-normalize.cc'
copyfile 'hb-ot-shape.cc'
copyfile 'hb-ot-tag.cc'
copyfile 'hb-ot-var.cc'
copyfile 'hb-set.cc'
copyfile 'hb-shape-plan.cc'
copyfile 'hb-shape.cc'
copyfile 'hb-shaper.cc'
copyfile 'hb-static.cc'
copyfile 'hb-subset-cff-common.cc'
copyfile 'hb-subset-cff1.cc'
copyfile 'hb-subset-cff2.cc'
copyfile 'hb-subset-input.cc'
copyfile 'hb-subset-plan.cc'
copyfile 'hb-subset-repacker.cc'
copyfile 'hb-subset.cc'
copyfile 'hb-ucd.cc'
copyfile 'hb-unicode.cc'
copyfile 'graph/gsubgpos-context.cc'
copyfile 'hb-paint.cc'
copyfile 'hb-paint-extents.cc'
copyfile 'hb-outline.cc'
copyfile 'hb-style.cc'
