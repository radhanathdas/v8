// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_CODE_H_
#define V8_OBJECTS_CODE_H_

#include "src/objects.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8 {
namespace internal {

class ByteArray;
class BytecodeArray;

// HandlerTable is a fixed array containing entries for exception handlers in
// the code object it is associated with. The tables comes in two flavors:
// 1) Based on ranges: Used for unoptimized code. Contains one entry per
//    exception handler and a range representing the try-block covered by that
//    handler. Layout looks as follows:
//      [ range-start , range-end , handler-offset , handler-data ]
// 2) Based on return addresses: Used for turbofanned code. Contains one entry
//    per call-site that could throw an exception. Layout looks as follows:
//      [ return-address-offset , handler-offset ]
class HandlerTable : public FixedArray {
 public:
  // Conservative prediction whether a given handler will locally catch an
  // exception or cause a re-throw to outside the code boundary. Since this is
  // undecidable it is merely an approximation (e.g. useful for debugger).
  enum CatchPrediction {
    UNCAUGHT,    // The handler will (likely) rethrow the exception.
    CAUGHT,      // The exception will be caught by the handler.
    PROMISE,     // The exception will be caught and cause a promise rejection.
    DESUGARING,  // The exception will be caught, but both the exception and the
                 // catching are part of a desugaring and should therefore not
                 // be visible to the user (we won't notify the debugger of such
                 // exceptions).
    ASYNC_AWAIT,  // The exception will be caught and cause a promise rejection
                  // in the desugaring of an async function, so special
                  // async/await handling in the debugger can take place.
  };

  // Getters for handler table based on ranges.
  inline int GetRangeStart(int index) const;
  inline int GetRangeEnd(int index) const;
  inline int GetRangeHandler(int index) const;
  inline int GetRangeData(int index) const;

  // Setters for handler table based on ranges.
  inline void SetRangeStart(int index, int value);
  inline void SetRangeEnd(int index, int value);
  inline void SetRangeHandler(int index, int offset, CatchPrediction pred);
  inline void SetRangeData(int index, int value);

  // Setters for handler table based on return addresses.
  inline void SetReturnOffset(int index, int value);
  inline void SetReturnHandler(int index, int offset);

  // Lookup handler in a table based on ranges. The {pc_offset} is an offset to
  // the start of the potentially throwing instruction (using return addresses
  // for this value would be invalid).
  int LookupRange(int pc_offset, int* data, CatchPrediction* prediction);

  // Lookup handler in a table based on return addresses.
  int LookupReturn(int pc_offset);

  // Returns the number of entries in the table.
  inline int NumberOfRangeEntries() const;

  // Returns the required length of the underlying fixed array.
  static int LengthForRange(int entries) { return entries * kRangeEntrySize; }
  static int LengthForReturn(int entries) { return entries * kReturnEntrySize; }

  // Returns an empty handler table.
  static Handle<HandlerTable> Empty(Isolate* isolate);

  DECL_CAST(HandlerTable)

#ifdef ENABLE_DISASSEMBLER
  void HandlerTableRangePrint(std::ostream& os);   // NOLINT
  void HandlerTableReturnPrint(std::ostream& os);  // NOLINT
#endif

 private:
  // Layout description for handler table based on ranges.
  static const int kRangeStartIndex = 0;
  static const int kRangeEndIndex = 1;
  static const int kRangeHandlerIndex = 2;
  static const int kRangeDataIndex = 3;
  static const int kRangeEntrySize = 4;

  // Layout description for handler table based on return addresses.
  static const int kReturnOffsetIndex = 0;
  static const int kReturnHandlerIndex = 1;
  static const int kReturnEntrySize = 2;

  // Encoding of the {handler} field.
  class HandlerPredictionField : public BitField<CatchPrediction, 0, 3> {};
  class HandlerOffsetField : public BitField<int, 3, 29> {};
};

// Code describes objects with on-the-fly generated machine code.
class Code : public HeapObject {
 public:
  // Opaque data type for encapsulating code flags like kind, inline
  // cache state, and arguments count.
  typedef uint32_t Flags;

#define CODE_KIND_LIST(V)   \
  V(OPTIMIZED_FUNCTION)     \
  V(BYTECODE_HANDLER)       \
  V(STUB)                   \
  V(BUILTIN)                \
  V(REGEXP)                 \
  V(WASM_FUNCTION)          \
  V(WASM_TO_JS_FUNCTION)    \
  V(JS_TO_WASM_FUNCTION)    \
  V(WASM_INTERPRETER_ENTRY) \
  V(C_WASM_ENTRY)

  enum Kind {
#define DEFINE_CODE_KIND_ENUM(name) name,
    CODE_KIND_LIST(DEFINE_CODE_KIND_ENUM)
#undef DEFINE_CODE_KIND_ENUM
        NUMBER_OF_KINDS
  };

  static const char* Kind2String(Kind kind);

#if defined(OBJECT_PRINT) || defined(ENABLE_DISASSEMBLER)
  // Printing
  static const char* ICState2String(InlineCacheState state);
#endif  // defined(OBJECT_PRINT) || defined(ENABLE_DISASSEMBLER)

#ifdef ENABLE_DISASSEMBLER
  void Disassemble(const char* name, std::ostream& os);  // NOLINT
#endif

  // [instruction_size]: Size of the native instructions
  inline int instruction_size() const;
  inline void set_instruction_size(int value);

  // [relocation_info]: Code relocation information
  DECL_ACCESSORS(relocation_info, ByteArray)
  void InvalidateRelocation();
  void InvalidateEmbeddedObjects();

  // [handler_table]: Fixed array containing offsets of exception handlers.
  DECL_ACCESSORS(handler_table, FixedArray)

  // [deoptimization_data]: Array containing data for deopt.
  DECL_ACCESSORS(deoptimization_data, FixedArray)

  // [source_position_table]: ByteArray for the source positions table or
  // SourcePositionTableWithFrameCache.
  DECL_ACCESSORS(source_position_table, Object)

  inline ByteArray* SourcePositionTable() const;

  // [trap_handler_index]: An index into the trap handler's master list of code
  // objects.
  DECL_ACCESSORS(trap_handler_index, Smi)

  // [raw_type_feedback_info]: This field stores various things, depending on
  // the kind of the code object.
  //   STUB and ICs       => major/minor key as Smi.
  // TODO(mvstanton): rename raw_type_feedback_info to stub_key, since the
  // field is no longer overloaded.
  DECL_ACCESSORS(raw_type_feedback_info, Object)
  inline uint32_t stub_key() const;
  inline void set_stub_key(uint32_t key);

  // [next_code_link]: Link for lists of optimized or deoptimized code.
  // Note that storage for this field is overlapped with typefeedback_info.
  DECL_ACCESSORS(next_code_link, Object)

  // [constant_pool offset]: Offset of the constant pool.
  // Valid for FLAG_enable_embedded_constant_pool only
  inline int constant_pool_offset() const;
  inline void set_constant_pool_offset(int offset);

  // Unchecked accessors to be used during GC.
  inline ByteArray* unchecked_relocation_info() const;

  inline int relocation_size() const;

  // [kind]: Access to specific code kind.
  inline Kind kind() const;
  inline void set_kind(Kind kind);

  inline bool is_stub() const;
  inline bool is_optimized_code() const;
  inline bool is_wasm_code() const;

  inline void set_raw_kind_specific_flags1(int value);
  inline void set_raw_kind_specific_flags2(int value);

  // Testers for interpreter builtins.
  inline bool is_interpreter_trampoline_builtin() const;

  // Tells whether the code checks the optimization marker in the function's
  // feedback vector.
  inline bool checks_optimization_marker() const;

  // [has_tagged_params]: For compiled code or builtins: Tells whether the
  // outgoing parameters of this code are tagged pointers. True for other kinds.
  inline bool has_tagged_params() const;
  inline void set_has_tagged_params(bool value);

  // [is_turbofanned]: For kind STUB or OPTIMIZED_FUNCTION, tells whether the
  // code object was generated by the TurboFan optimizing compiler.
  inline bool is_turbofanned() const;
  inline void set_is_turbofanned(bool value);

  // [can_have_weak_objects]: For kind OPTIMIZED_FUNCTION, tells whether the
  // embedded objects in code should be treated weakly.
  inline bool can_have_weak_objects() const;
  inline void set_can_have_weak_objects(bool value);

  // [is_construct_stub]: For kind BUILTIN, tells whether the code object
  // represents a hand-written construct stub
  // (e.g., NumberConstructor_ConstructStub).
  inline bool is_construct_stub() const;
  inline void set_is_construct_stub(bool value);

  // [builtin_index]: For builtins, tells which builtin index the code object
  // has. The builtin index is a non-negative integer for builtins, and -1
  // otherwise.
  inline int builtin_index() const;
  inline void set_builtin_index(int id);
  inline bool is_builtin() const;

  // [stack_slots]: For kind OPTIMIZED_FUNCTION, the number of stack slots
  // reserved in the code prologue.
  inline unsigned stack_slots() const;
  inline void set_stack_slots(unsigned slots);

  // [safepoint_table_start]: For kind OPTIMIZED_FUNCTION, the offset in
  // the instruction stream where the safepoint table starts.
  inline unsigned safepoint_table_offset() const;
  inline void set_safepoint_table_offset(unsigned offset);

  // [marked_for_deoptimization]: For kind OPTIMIZED_FUNCTION tells whether
  // the code is going to be deoptimized because of dead embedded maps.
  inline bool marked_for_deoptimization() const;
  inline void set_marked_for_deoptimization(bool flag);

  // [deopt_already_counted]: For kind OPTIMIZED_FUNCTION tells whether
  // the code was already deoptimized.
  inline bool deopt_already_counted() const;
  inline void set_deopt_already_counted(bool flag);

  // [is_promise_rejection]: For kind BUILTIN tells whether the
  // exception thrown by the code will lead to promise rejection or
  // uncaught if both this and is_exception_caught is set.
  // Use GetBuiltinCatchPrediction to access this.
  inline void set_is_promise_rejection(bool flag);

  // [is_exception_caught]: For kind BUILTIN tells whether the
  // exception thrown by the code will be caught internally or
  // uncaught if both this and is_promise_rejection is set.
  // Use GetBuiltinCatchPrediction to access this.
  inline void set_is_exception_caught(bool flag);

  // [constant_pool]: The constant pool for this function.
  inline Address constant_pool();

  // Get the safepoint entry for the given pc.
  SafepointEntry GetSafepointEntry(Address pc);

  // The entire code object including its header is copied verbatim to the
  // snapshot so that it can be written in one, fast, memcpy during
  // deserialization. The deserializer will overwrite some pointers, rather
  // like a runtime linker, but the random allocation addresses used in the
  // mksnapshot process would still be present in the unlinked snapshot data,
  // which would make snapshot production non-reproducible. This method wipes
  // out the to-be-overwritten header data for reproducible snapshots.
  inline void WipeOutHeader();

  // Clear uninitialized padding space. This ensures that the snapshot content
  // is deterministic.
  inline void clear_padding();
  // Initialize the flags field. Similar to clear_padding above this ensure that
  // the snapshot content is deterministic.
  inline void initialize_flags(Kind kind);

  // Convert a target address into a code object.
  static inline Code* GetCodeFromTargetAddress(Address address);

  // Convert an entry address into an object.
  static inline Object* GetObjectFromEntryAddress(Address location_of_address);

  // Convert a code entry into an object.
  static inline Object* GetObjectFromCodeEntry(Address code_entry);

  // Returns the address of the first instruction.
  inline byte* instruction_start() const;

  // Returns the address right after the last instruction.
  inline byte* instruction_end() const;

  // Returns the size of the instructions, padding, relocation and unwinding
  // information.
  inline int body_size() const;

  // Returns the size of code and its metadata. This includes the size of code
  // relocation information, deoptimization data and handler table.
  inline int SizeIncludingMetadata() const;

  // Returns the address of the first relocation info (read backwards!).
  inline byte* relocation_start() const;

  // [has_unwinding_info]: Whether this code object has unwinding information.
  // If it doesn't, unwinding_information_start() will point to invalid data.
  //
  // The body of all code objects has the following layout.
  //
  //  +--------------------------+  <-- instruction_start()
  //  |       instructions       |
  //  |           ...            |
  //  +--------------------------+
  //  |      relocation info     |
  //  |           ...            |
  //  +--------------------------+  <-- instruction_end()
  //
  // If has_unwinding_info() is false, instruction_end() points to the first
  // memory location after the end of the code object. Otherwise, the body
  // continues as follows:
  //
  //  +--------------------------+
  //  |    padding to the next   |
  //  |  8-byte aligned address  |
  //  +--------------------------+  <-- instruction_end()
  //  |   [unwinding_info_size]  |
  //  |        as uint64_t       |
  //  +--------------------------+  <-- unwinding_info_start()
  //  |       unwinding info     |
  //  |            ...           |
  //  +--------------------------+  <-- unwinding_info_end()
  //
  // and unwinding_info_end() points to the first memory location after the end
  // of the code object.
  //
  DECL_BOOLEAN_ACCESSORS(has_unwinding_info)

  // [unwinding_info_size]: Size of the unwinding information.
  inline int unwinding_info_size() const;
  inline void set_unwinding_info_size(int value);

  // Returns the address of the unwinding information, if any.
  inline byte* unwinding_info_start() const;

  // Returns the address right after the end of the unwinding information.
  inline byte* unwinding_info_end() const;

  // Code entry point.
  inline byte* entry() const;

  // Returns true if pc is inside this object's instructions.
  inline bool contains(byte* pc);

  // Relocate the code by delta bytes. Called to signal that this code
  // object has been moved by delta bytes.
  void Relocate(intptr_t delta);

  // Migrate code described by desc.
  void CopyFrom(const CodeDesc& desc);

  // Returns the object size for a given body (used for allocation).
  static int SizeFor(int body_size) {
    DCHECK_SIZE_TAG_ALIGNED(body_size);
    return RoundUp(kHeaderSize + body_size, kCodeAlignment);
  }

  // Calculate the size of the code object to report for log events. This takes
  // the layout of the code object into account.
  inline int ExecutableSize() const;

  DECL_CAST(Code)

  // Dispatched behavior.
  inline int CodeSize() const;

  DECL_PRINTER(Code)
  DECL_VERIFIER(Code)

  void PrintDeoptLocation(FILE* out, Address pc);
  bool CanDeoptAt(Address pc);

  inline HandlerTable::CatchPrediction GetBuiltinCatchPrediction();
#ifdef VERIFY_HEAP
  void VerifyEmbeddedObjectsDependency();
#endif

#ifdef DEBUG
  enum VerifyMode { kNoContextSpecificPointers, kNoContextRetainingPointers };
  void VerifyEmbeddedObjects(VerifyMode mode = kNoContextRetainingPointers);
#endif  // DEBUG

  inline bool CanContainWeakObjects();

  inline bool IsWeakObject(Object* object);

  static inline bool IsWeakObjectInOptimizedCode(Object* object);

  static Handle<WeakCell> WeakCellFor(Handle<Code> code);
  WeakCell* CachedWeakCell();

  // Return true if the function is inlined in the code.
  bool Inlines(SharedFunctionInfo* sfi);

  class OptimizedCodeIterator {
   public:
    explicit OptimizedCodeIterator(Isolate* isolate);
    Code* Next();

   private:
    Context* next_context_;
    Code* current_code_;
    Isolate* isolate_;

    DisallowHeapAllocation no_gc;
    DISALLOW_COPY_AND_ASSIGN(OptimizedCodeIterator)
  };

  static const int kConstantPoolSize =
      FLAG_enable_embedded_constant_pool ? kIntSize : 0;

  // Layout description.
  static const int kRelocationInfoOffset = HeapObject::kHeaderSize;
  static const int kHandlerTableOffset = kRelocationInfoOffset + kPointerSize;
  static const int kDeoptimizationDataOffset =
      kHandlerTableOffset + kPointerSize;
  static const int kSourcePositionTableOffset =
      kDeoptimizationDataOffset + kPointerSize;
  // For FUNCTION kind, we store the type feedback info here.
  static const int kTypeFeedbackInfoOffset =
      kSourcePositionTableOffset + kPointerSize;
  static const int kNextCodeLinkOffset = kTypeFeedbackInfoOffset + kPointerSize;
  static const int kInstructionSizeOffset = kNextCodeLinkOffset + kPointerSize;
  static const int kFlagsOffset = kInstructionSizeOffset + kIntSize;
  static const int kKindSpecificFlags1Offset = kFlagsOffset + kIntSize;
  static const int kKindSpecificFlags2Offset =
      kKindSpecificFlags1Offset + kIntSize;
  static const int kConstantPoolOffset = kKindSpecificFlags2Offset + kIntSize;
  static const int kBuiltinIndexOffset =
      kConstantPoolOffset + kConstantPoolSize;
  static const int kTrapHandlerIndex = kBuiltinIndexOffset + kIntSize;
  static const int kHeaderPaddingStart = kTrapHandlerIndex + kPointerSize;

  // Add padding to align the instruction start following right after
  // the Code object header.
  static const int kHeaderSize =
      (kHeaderPaddingStart + kCodeAlignmentMask) & ~kCodeAlignmentMask;

  // Data or code not directly visited by GC directly starts here.
  // The serializer needs to copy bytes starting from here verbatim.
  // Objects embedded into code is visited via reloc info.
  static const int kDataStart = kInstructionSizeOffset;

  inline int GetUnwindingInfoSizeOffset() const;

  class BodyDescriptor;

  // Flags layout.  BitField<type, shift, size>.
  class HasUnwindingInfoField : public BitField<bool, 0, 1> {};
  class KindField : public BitField<Kind, HasUnwindingInfoField::kNext, 5> {};
  STATIC_ASSERT(NUMBER_OF_KINDS <= KindField::kMax);

  // KindSpecificFlags1 layout (STUB, BUILTIN and OPTIMIZED_FUNCTION)
  static const int kStackSlotsFirstBit = 0;
  static const int kStackSlotsBitCount = 24;
  static const int kMarkedForDeoptimizationBit =
      kStackSlotsFirstBit + kStackSlotsBitCount;
  static const int kDeoptAlreadyCountedBit = kMarkedForDeoptimizationBit + 1;
  static const int kIsTurbofannedBit = kDeoptAlreadyCountedBit + 1;
  static const int kCanHaveWeakObjects = kIsTurbofannedBit + 1;
  // Could be moved to overlap previous bits when we need more space.
  static const int kIsConstructStub = kCanHaveWeakObjects + 1;
  static const int kIsPromiseRejection = kIsConstructStub + 1;
  static const int kIsExceptionCaught = kIsPromiseRejection + 1;

  STATIC_ASSERT(kStackSlotsFirstBit + kStackSlotsBitCount <= 32);
  STATIC_ASSERT(kIsExceptionCaught + 1 <= 32);

  class StackSlotsField
      : public BitField<int, kStackSlotsFirstBit, kStackSlotsBitCount> {
  };  // NOLINT
  class MarkedForDeoptimizationField
      : public BitField<bool, kMarkedForDeoptimizationBit, 1> {};  // NOLINT
  class DeoptAlreadyCountedField
      : public BitField<bool, kDeoptAlreadyCountedBit, 1> {};  // NOLINT
  class IsTurbofannedField : public BitField<bool, kIsTurbofannedBit, 1> {
  };  // NOLINT
  class CanHaveWeakObjectsField
      : public BitField<bool, kCanHaveWeakObjects, 1> {};  // NOLINT
  class IsConstructStubField : public BitField<bool, kIsConstructStub, 1> {
  };  // NOLINT
  class IsPromiseRejectionField
      : public BitField<bool, kIsPromiseRejection, 1> {};  // NOLINT
  class IsExceptionCaughtField : public BitField<bool, kIsExceptionCaught, 1> {
  };  // NOLINT

  // KindSpecificFlags2 layout (ALL)
  static const int kHasTaggedStackBit = 0;
  class HasTaggedStackField : public BitField<bool, kHasTaggedStackBit, 1> {};

  // KindSpecificFlags2 layout (STUB and OPTIMIZED_FUNCTION)
  static const int kSafepointTableOffsetFirstBit = kHasTaggedStackBit + 1;
  static const int kSafepointTableOffsetBitCount = 30;

  STATIC_ASSERT(kSafepointTableOffsetFirstBit + kSafepointTableOffsetBitCount <=
                32);
  STATIC_ASSERT(1 + kSafepointTableOffsetBitCount <= 32);

  class SafepointTableOffsetField
      : public BitField<int, kSafepointTableOffsetFirstBit,
                        kSafepointTableOffsetBitCount> {};  // NOLINT

  static const int kArgumentsBits = 16;
  static const int kMaxArguments = (1 << kArgumentsBits) - 1;

 private:
  friend class RelocIterator;

  bool is_promise_rejection() const;
  bool is_exception_caught() const;

  DISALLOW_IMPLICIT_CONSTRUCTORS(Code);
};

class AbstractCode : public HeapObject {
 public:
  // All code kinds and INTERPRETED_FUNCTION.
  enum Kind {
#define DEFINE_CODE_KIND_ENUM(name) name,
    CODE_KIND_LIST(DEFINE_CODE_KIND_ENUM)
#undef DEFINE_CODE_KIND_ENUM
        INTERPRETED_FUNCTION,
    NUMBER_OF_KINDS
  };

  static const char* Kind2String(Kind kind);

  int SourcePosition(int offset);
  int SourceStatementPosition(int offset);

  // Returns the address of the first instruction.
  inline Address instruction_start();

  // Returns the address right after the last instruction.
  inline Address instruction_end();

  // Returns the size of the code instructions.
  inline int instruction_size();

  // Return the source position table.
  inline ByteArray* source_position_table();

  inline Object* stack_frame_cache();
  static void SetStackFrameCache(Handle<AbstractCode> abstract_code,
                                 Handle<UnseededNumberDictionary> cache);
  void DropStackFrameCache();

  // Returns the size of instructions and the metadata.
  inline int SizeIncludingMetadata();

  // Returns true if pc is inside this object's instructions.
  inline bool contains(byte* pc);

  // Returns the AbstractCode::Kind of the code.
  inline Kind kind();

  // Calculate the size of the code object to report for log events. This takes
  // the layout of the code object into account.
  inline int ExecutableSize();

  DECL_CAST(AbstractCode)
  inline Code* GetCode();
  inline BytecodeArray* GetBytecodeArray();

  // Max loop nesting marker used to postpose OSR. We don't take loop
  // nesting that is deeper than 5 levels into account.
  static const int kMaxLoopNestingMarker = 6;
};

// Dependent code is a singly linked list of fixed arrays. Each array contains
// code objects in weak cells for one dependent group. The suffix of the array
// can be filled with the undefined value if the number of codes is less than
// the length of the array.
//
// +------+-----------------+--------+--------+-----+--------+-----------+-----+
// | next | count & group 1 | code 1 | code 2 | ... | code n | undefined | ... |
// +------+-----------------+--------+--------+-----+--------+-----------+-----+
//    |
//    V
// +------+-----------------+--------+--------+-----+--------+-----------+-----+
// | next | count & group 2 | code 1 | code 2 | ... | code m | undefined | ... |
// +------+-----------------+--------+--------+-----+--------+-----------+-----+
//    |
//    V
// empty_fixed_array()
//
// The list of fixed arrays is ordered by dependency groups.

class DependentCode : public FixedArray {
 public:
  enum DependencyGroup {
    // Group of code that weakly embed this map and depend on being
    // deoptimized when the map is garbage collected.
    kWeakCodeGroup,
    // Group of code that embed a transition to this map, and depend on being
    // deoptimized when the transition is replaced by a new version.
    kTransitionGroup,
    // Group of code that omit run-time prototype checks for prototypes
    // described by this map. The group is deoptimized whenever an object
    // described by this map changes shape (and transitions to a new map),
    // possibly invalidating the assumptions embedded in the code.
    kPrototypeCheckGroup,
    // Group of code that depends on global property values in property cells
    // not being changed.
    kPropertyCellChangedGroup,
    // Group of code that omit run-time checks for field(s) introduced by
    // this map, i.e. for the field type.
    kFieldOwnerGroup,
    // Group of code that omit run-time type checks for initial maps of
    // constructors.
    kInitialMapChangedGroup,
    // Group of code that depends on tenuring information in AllocationSites
    // not being changed.
    kAllocationSiteTenuringChangedGroup,
    // Group of code that depends on element transition information in
    // AllocationSites not being changed.
    kAllocationSiteTransitionChangedGroup
  };

  static const int kGroupCount = kAllocationSiteTransitionChangedGroup + 1;
  static const int kNextLinkIndex = 0;
  static const int kFlagsIndex = 1;
  static const int kCodesStartIndex = 2;

  bool Contains(DependencyGroup group, WeakCell* code_cell);
  bool IsEmpty(DependencyGroup group);

  static Handle<DependentCode> InsertCompilationDependencies(
      Handle<DependentCode> entries, DependencyGroup group,
      Handle<Foreign> info);

  static Handle<DependentCode> InsertWeakCode(Handle<DependentCode> entries,
                                              DependencyGroup group,
                                              Handle<WeakCell> code_cell);

  void UpdateToFinishedCode(DependencyGroup group, Foreign* info,
                            WeakCell* code_cell);

  void RemoveCompilationDependencies(DependentCode::DependencyGroup group,
                                     Foreign* info);

  void DeoptimizeDependentCodeGroup(Isolate* isolate,
                                    DependentCode::DependencyGroup group);

  bool MarkCodeForDeoptimization(Isolate* isolate,
                                 DependentCode::DependencyGroup group);

  // The following low-level accessors should only be used by this class
  // and the mark compact collector.
  inline DependentCode* next_link();
  inline void set_next_link(DependentCode* next);
  inline int count();
  inline void set_count(int value);
  inline DependencyGroup group();
  inline void set_group(DependencyGroup group);
  inline Object* object_at(int i);
  inline void set_object_at(int i, Object* object);
  inline void clear_at(int i);
  inline void copy(int from, int to);
  DECL_CAST(DependentCode)

  static const char* DependencyGroupName(DependencyGroup group);
  static void SetMarkedForDeoptimization(Code* code, DependencyGroup group);

 private:
  static Handle<DependentCode> Insert(Handle<DependentCode> entries,
                                      DependencyGroup group,
                                      Handle<Object> object);
  static Handle<DependentCode> New(DependencyGroup group, Handle<Object> object,
                                   Handle<DependentCode> next);
  static Handle<DependentCode> EnsureSpace(Handle<DependentCode> entries);
  // Compact by removing cleared weak cells and return true if there was
  // any cleared weak cell.
  bool Compact();
  static int Grow(int number_of_entries) {
    if (number_of_entries < 5) return number_of_entries + 1;
    return number_of_entries * 5 / 4;
  }
  inline int flags();
  inline void set_flags(int flags);
  class GroupField : public BitField<int, 0, 3> {};
  class CountField : public BitField<int, 3, 27> {};
  STATIC_ASSERT(kGroupCount <= GroupField::kMax + 1);
};

// BytecodeArray represents a sequence of interpreter bytecodes.
class BytecodeArray : public FixedArrayBase {
 public:
  enum Age {
    kNoAgeBytecodeAge = 0,
    kQuadragenarianBytecodeAge,
    kQuinquagenarianBytecodeAge,
    kSexagenarianBytecodeAge,
    kSeptuagenarianBytecodeAge,
    kOctogenarianBytecodeAge,
    kAfterLastBytecodeAge,
    kFirstBytecodeAge = kNoAgeBytecodeAge,
    kLastBytecodeAge = kAfterLastBytecodeAge - 1,
    kBytecodeAgeCount = kAfterLastBytecodeAge - kFirstBytecodeAge - 1,
    kIsOldBytecodeAge = kSexagenarianBytecodeAge
  };

  static int SizeFor(int length) {
    return OBJECT_POINTER_ALIGN(kHeaderSize + length);
  }

  // Setter and getter
  inline byte get(int index);
  inline void set(int index, byte value);

  // Returns data start address.
  inline Address GetFirstBytecodeAddress();

  // Accessors for frame size.
  inline int frame_size() const;
  inline void set_frame_size(int frame_size);

  // Accessor for register count (derived from frame_size).
  inline int register_count() const;

  // Accessors for parameter count (including implicit 'this' receiver).
  inline int parameter_count() const;
  inline void set_parameter_count(int number_of_parameters);

  // Register used to pass the incoming new.target or generator object from the
  // fucntion call.
  inline interpreter::Register incoming_new_target_or_generator_register()
      const;
  inline void set_incoming_new_target_or_generator_register(
      interpreter::Register incoming_new_target_or_generator_register);

  // Accessors for profiling count.
  inline int interrupt_budget() const;
  inline void set_interrupt_budget(int interrupt_budget);

  // Accessors for OSR loop nesting level.
  inline int osr_loop_nesting_level() const;
  inline void set_osr_loop_nesting_level(int depth);

  // Accessors for bytecode's code age.
  inline Age bytecode_age() const;
  inline void set_bytecode_age(Age age);

  // Accessors for the constant pool.
  DECL_ACCESSORS(constant_pool, FixedArray)

  // Accessors for handler table containing offsets of exception handlers.
  DECL_ACCESSORS(handler_table, FixedArray)

  // Accessors for source position table containing mappings between byte code
  // offset and source position or SourcePositionTableWithFrameCache.
  DECL_ACCESSORS(source_position_table, Object)

  inline ByteArray* SourcePositionTable();

  DECL_CAST(BytecodeArray)

  // Dispatched behavior.
  inline int BytecodeArraySize();

  inline int instruction_size();

  // Returns the size of bytecode and its metadata. This includes the size of
  // bytecode, constant pool, source position table, and handler table.
  inline int SizeIncludingMetadata();

  int SourcePosition(int offset);
  int SourceStatementPosition(int offset);

  DECL_PRINTER(BytecodeArray)
  DECL_VERIFIER(BytecodeArray)

  void Disassemble(std::ostream& os);

  void CopyBytecodesTo(BytecodeArray* to);

  // Bytecode aging
  bool IsOld() const;
  void MakeOlder();

  // Clear uninitialized padding space. This ensures that the snapshot content
  // is deterministic.
  inline void clear_padding();

// Layout description.
#define BYTECODE_ARRAY_FIELDS(V)                           \
  /* Pointer fields. */                                    \
  V(kConstantPoolOffset, kPointerSize)                     \
  V(kHandlerTableOffset, kPointerSize)                     \
  V(kSourcePositionTableOffset, kPointerSize)              \
  V(kFrameSizeOffset, kIntSize)                            \
  V(kParameterSizeOffset, kIntSize)                        \
  V(kIncomingNewTargetOrGeneratorRegisterOffset, kIntSize) \
  V(kInterruptBudgetOffset, kIntSize)                      \
  V(kOSRNestingLevelOffset, kCharSize)                     \
  V(kBytecodeAgeOffset, kCharSize)                         \
  /* Total size. */                                        \
  V(kHeaderSize, 0)

  DEFINE_FIELD_OFFSET_CONSTANTS(FixedArrayBase::kHeaderSize,
                                BYTECODE_ARRAY_FIELDS)
#undef BYTECODE_ARRAY_FIELDS

  // Maximal memory consumption for a single BytecodeArray.
  static const int kMaxSize = 512 * MB;
  // Maximal length of a single BytecodeArray.
  static const int kMaxLength = kMaxSize - kHeaderSize;

  class BodyDescriptor;
  // No weak fields.
  typedef BodyDescriptor BodyDescriptorWeak;

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(BytecodeArray);
};

// DeoptimizationData is a fixed array used to hold the deoptimization data for
// optimized code.  It also contains information about functions that were
// inlined.  If N different functions were inlined then the first N elements of
// the literal array will contain these functions.
//
// It can be empty.
class DeoptimizationData : public FixedArray {
 public:
  // Layout description.  Indices in the array.
  static const int kTranslationByteArrayIndex = 0;
  static const int kInlinedFunctionCountIndex = 1;
  static const int kLiteralArrayIndex = 2;
  static const int kOsrBytecodeOffsetIndex = 3;
  static const int kOsrPcOffsetIndex = 4;
  static const int kOptimizationIdIndex = 5;
  static const int kSharedFunctionInfoIndex = 6;
  static const int kWeakCellCacheIndex = 7;
  static const int kInliningPositionsIndex = 8;
  static const int kFirstDeoptEntryIndex = 9;

  // Offsets of deopt entry elements relative to the start of the entry.
  static const int kBytecodeOffsetRawOffset = 0;
  static const int kTranslationIndexOffset = 1;
  static const int kPcOffset = 2;
  static const int kDeoptEntrySize = 3;

// Simple element accessors.
#define DECL_ELEMENT_ACCESSORS(name, type) \
  inline type* name();                     \
  inline void Set##name(type* value);

  DECL_ELEMENT_ACCESSORS(TranslationByteArray, ByteArray)
  DECL_ELEMENT_ACCESSORS(InlinedFunctionCount, Smi)
  DECL_ELEMENT_ACCESSORS(LiteralArray, FixedArray)
  DECL_ELEMENT_ACCESSORS(OsrBytecodeOffset, Smi)
  DECL_ELEMENT_ACCESSORS(OsrPcOffset, Smi)
  DECL_ELEMENT_ACCESSORS(OptimizationId, Smi)
  DECL_ELEMENT_ACCESSORS(SharedFunctionInfo, Object)
  DECL_ELEMENT_ACCESSORS(WeakCellCache, Object)
  DECL_ELEMENT_ACCESSORS(InliningPositions, PodArray<InliningPosition>)

#undef DECL_ELEMENT_ACCESSORS

// Accessors for elements of the ith deoptimization entry.
#define DECL_ENTRY_ACCESSORS(name, type) \
  inline type* name(int i);              \
  inline void Set##name(int i, type* value);

  DECL_ENTRY_ACCESSORS(BytecodeOffsetRaw, Smi)
  DECL_ENTRY_ACCESSORS(TranslationIndex, Smi)
  DECL_ENTRY_ACCESSORS(Pc, Smi)

#undef DECL_ENTRY_ACCESSORS

  inline BailoutId BytecodeOffset(int i);

  inline void SetBytecodeOffset(int i, BailoutId value);

  inline int DeoptCount();

  static const int kNotInlinedIndex = -1;

  // Returns the inlined function at the given position in LiteralArray, or the
  // outer function if index == kNotInlinedIndex.
  class SharedFunctionInfo* GetInlinedFunction(int index);

  // Allocates a DeoptimizationData.
  static Handle<DeoptimizationData> New(Isolate* isolate, int deopt_entry_count,
                                        PretenureFlag pretenure);

  // Return an empty DeoptimizationData.
  static Handle<DeoptimizationData> Empty(Isolate* isolate);

  DECL_CAST(DeoptimizationData)

#ifdef ENABLE_DISASSEMBLER
  void DeoptimizationDataPrint(std::ostream& os);  // NOLINT
#endif

 private:
  static int IndexForEntry(int i) {
    return kFirstDeoptEntryIndex + (i * kDeoptEntrySize);
  }

  static int LengthFor(int entry_count) { return IndexForEntry(entry_count); }
};

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_CODE_H_
