// REQUIRES: shell
//
// RUN: cd %S
// RUN: rm -f %t.cpm %t-base.pcm %t-base.d %t.d
// RUN: %clang_cc1 -I. -x c++ -fmodule-maps -fmodule-name=test-base -fno-modules-implicit-maps -fmodules -emit-module -fno-validate-pch -fmodules-strict-decluse Inputs/dependency-gen-base.modulemap -dependency-file %t-base.d -MT %t-base.pcm -o %t-base.pcm -fmodule-map-file-home-is-cwd
// RUN: %clang_cc1 -I. -x c++ -fmodule-maps -fmodule-name=test -fno-modules-implicit-maps -fmodules -emit-module -fno-validate-pch -fmodules-strict-decluse -fmodule-file=%t-base.pcm dependency-gen.modulemap.cpp -dependency-file %t.d -MT %t.pcm -o %t.pcm -fmodule-map-file-home-is-cwd
// RUN: FileCheck %s < %t.d
module "test" {
  export *
  header "Inputs/dependency-gen.h"
  use "test-base"
  use "test-base2"
}
extern module "test-base2" "Inputs/dependency-gen-base2.modulemap"
extern module "test-base" "Inputs/dependency-gen-base.modulemap"

// CHECK: {{ |\./}}Inputs/dependency-gen-included2.h
// CHECK: {{ |\./}}Inputs/dependency-gen-base.modulemap
