# adir01pcpp
Bit Trade Oneの製品であるUSB赤外線リモコンアドバンス(ADIR01P)をC++から使えるようにしたライブラリ。
リモコンから発せられる信号の読み取りと, 読み取った信号を発信することができる。

## ビルドに必要なもの
g++
CMake
libusb 1.0以上

Arch LinuxをインストールしたRaspberry PI 2で動作確認済み。

## ビルド方法
```console
$ mkdir build
$ cd build
$ cmake ../adir01pcpp
```

## サンプルプログラムの使い方
実行にはroot権限が必要。
赤外線信号の読み取り。プログラムを実行してから5秒間信号待ち状態になる。読み取った結果はファイルに保存される。
```console
# ./adir01psend r file
```

赤外線信号を発信。読み取った赤外線信号が記録されたファイルを指定する。
```console
# ./adir01psend s file
```

複数ファイルを指定することによって連続して信号を送信することが可能。
```console
# ./adir01psend s file0 file1
```

## ライブラリ使用方法
include/adir01pcpp.hppをインクルードしsrc/adir01pcpp.cppをリンクする。
adir01pcppのインスタンスからreadIRDataを呼ぶと信号の読み取り、sendIRで信号の送信ができる。
詳しくはinclude/adir01pcpp.hppやexample/adir01psend.cppを参照。

This software is released under the MIT License, see LICENSE.
