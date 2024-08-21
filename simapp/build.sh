#!/bin/bash

# 安装依赖
cnpm install
cnpm i @vue/shared

# 使用旧版本的依赖
cd desktop
cnpm i
cd ..
node ./scripts/prepare.mjs

#打包
npm run build:scene
npm run build:map
npm run build:desktop
