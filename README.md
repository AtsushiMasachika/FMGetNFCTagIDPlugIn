# FMGetNFCTagIDPlugIn
![](https://img.shields.io/badge/FileMaker-gray)
![](https://img.shields.io/badge/PlugIn-black)
![](https://img.shields.io/badge/C++-F34B7D)

## Overview
This PlugIn get and returns a UID when an NFC tag is held over an NFC-enabled card reader.</br>
Currently, only Windows is supported.</br>

## How to use

After building the source file and installing the plugin, GetNFCTagID ( timeoutseconds ) is added to the function.</br>
Pass an integer value between 1 and 10 seconds as the "timeoutseconds" argument.</br>
It is the time to wait for the NFC-enabled card reader to read the tag.</br>

Please prepare your own library files.<br>

## If you want to use only the PlugIn

Please use the files under ActualFmxlFiles.
