// RUN: %target-swift-emit-sil -I %S/Inputs -enable-cxx-interop %s | %FileCheck %s

import Reference

func getCxxRef() -> UnsafeMutablePointer<CInt> {
  return getStaticIntRef()
}

// CHECK: sil hidden @$s4main9getCxxRefSpys5Int32VGyF : $@convention(thin) () -> UnsafeMutablePointer<Int32>
// CHECK: [[REF:%.*]] = function_ref @{{_Z15getStaticIntRefv|\?getStaticIntRef@@YAAEAHXZ}} : $@convention(c) () -> UnsafeMutablePointer<Int32>
// CHECK: apply [[REF]]() : $@convention(c) () -> UnsafeMutablePointer<Int32>

func getConstCxxRef() -> UnsafePointer<CInt> {
  return getConstStaticIntRef()
}

// CHECK: sil hidden @$s4main14getConstCxxRefSPys5Int32VGyF : $@convention(thin) () -> UnsafePointer<Int32>
// CHECK: [[REF:%.*]] = function_ref @{{_Z20getConstStaticIntRefv|\?getConstStaticIntRef@@YAAEBHXZ}} : $@convention(c) () -> UnsafePointer<Int32>
// CHECK: apply [[REF]]() : $@convention(c) () -> UnsafePointer<Int32>

func getCxxRvalueRef() -> UnsafeMutablePointer<CInt> {
  return getStaticIntRvalueRef()
}

// CHECK: sil hidden @$s4main15getCxxRvalueRefSpys5Int32VGyF : $@convention(thin) () -> UnsafeMutablePointer<Int32>
// CHECK: [[REF:%.*]] = function_ref @{{_Z21getStaticIntRvalueRefv|\?getStaticIntRvalueRef@@YA\$\$QEAHXZ}} : $@convention(c) () -> UnsafeMutablePointer<Int32>
// CHECK: apply [[REF]]() : $@convention(c) () -> UnsafeMutablePointer<Int32>

func getConstCxxRvalueRef() -> UnsafePointer<CInt> {
  return getConstStaticIntRvalueRef()
}

// CHECK: sil hidden @$s4main20getConstCxxRvalueRefSPys5Int32VGyF : $@convention(thin) () -> UnsafePointer<Int32>
// CHECK: [[REF:%.*]] = function_ref @{{_Z26getConstStaticIntRvalueRefv|\?getConstStaticIntRvalueRef@@YA\$\$QEBHXZ}} : $@convention(c) () -> UnsafePointer<Int32>
// CHECK: apply [[REF]]() : $@convention(c) () -> UnsafePointer<Int32>

func setCxxRef() {
  var val: CInt = 21
  setStaticIntRef(&val)
}

// CHECK: sil hidden @$s4main9setCxxRefyyF : $@convention(thin) () -> ()
// CHECK: [[REF:%.*]] = function_ref @{{_Z15setStaticIntRefRi|\?setStaticIntRef@@YAXAEAH@Z}} : $@convention(c) (@inout Int32) -> ()
// CHECK: apply [[REF]](%{{[0-9]+}}) : $@convention(c) (@inout Int32) -> ()

func setCxxConstRef() {
  let val: CInt = 21
  setConstStaticIntRef(val)
}

// CHECK: sil hidden @$s4main14setCxxConstRefyyF : $@convention(thin) () -> ()
// CHECK: [[REF:%.*]] = function_ref @{{_Z20setConstStaticIntRefRKi|\?setConstStaticIntRef@@YAXAEBH@Z}} : $@convention(c) (@in Int32) -> ()
// CHECK: apply [[REF]](%{{[0-9]+}}) : $@convention(c) (@in Int32) -> ()

func setCxxRvalueRef() {
  var val: CInt = 21
  setStaticIntRvalueRef(&val)
}

// CHECK: sil hidden @$s4main15setCxxRvalueRefyyF : $@convention(thin) () -> ()
// CHECK: [[REF:%.*]] = function_ref @{{_Z21setStaticIntRvalueRefOi|\?setStaticIntRvalueRef@@YAX\$\$QEAH@Z}} : $@convention(c) (@inout Int32) -> ()
// CHECK: apply [[REF]](%{{[0-9]+}}) : $@convention(c) (@inout Int32) -> ()

func setCxxConstRvalueRef() {
  let val: CInt = 21
  setConstStaticIntRvalueRef(val)
}

// CHECK: sil hidden @$s4main20setCxxConstRvalueRefyyF : $@convention(thin) () -> ()
// CHECK: [[REF:%.*]] = function_ref @{{_Z26setConstStaticIntRvalueRefOKi|\?setConstStaticIntRvalueRef@@YAX\$\$QEBH@Z}} : $@convention(c) (@in Int32) -> ()
// CHECK: apply [[REF]](%{{[0-9]+}}) : $@convention(c) (@in Int32) -> ()
