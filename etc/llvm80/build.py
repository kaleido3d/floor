#!/usr/bin/python bash
import os, sys

pushstack = list()

def pushdir(dirname):
  global pushstack
  pushstack.append(os.getcwd())
  os.chdir(dirname)

def popdir():
  global pushstack
  os.chdir(pushstack.pop())

def create_repo(in_dir, remote, commit):
    if not os.path.exists(in_dir):
        os.makedirs(in_dir)
    pushdir(in_dir)    
    os.system('git init')
    os.system('git add origin %s' % remote)
    os.system('git fetch --depth=25000 origin master')
    os.system('git reset --hard %s' % commit)
    popdir()

def clone_to(remote, cdir):
    os.system('git clone %s %s' % (remote, cdir))

create_repo('llvm/tools/clang', 'https://github.com/llvm-mirror/clang.git', 'a6b9739069763243020f4ea6fe586bc135fde1f9')
create_repo('llvm', 'https://github.com/llvm-mirror/llvm.git', '032b00a5404865765cda7db3039f39d54964d8b0')
create_repo('libcxx', 'https://github.com/llvm-mirror/libcxx.git', 'a443a0013d336593743fa1d523f2ee428814beb1')

clone_to('https://github.com/a2flo/SPIRV-Tools.git', '.')
clone_to('https://github.com/KhronosGroup/SPIRV-Headers.git', 'SPIRV-Tools/external/spirv-headers')

os.chdir('llvm')
os.system('git apply -p1 --ignore-whitespace ../80_llvm.patch')

os.chdir('tools/clang')
os.system('git apply -p1 --ignore-whitespace ../../../80_clang.patch')

os.chdir('../../../libcxx')
os.system('git apply -p1 --ignore-whitespace ../80_libcxx.patch')
os.chdir('..')