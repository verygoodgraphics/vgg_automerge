cmake_minimum_required (VERSION 3.12)

add_library (automerge STATIC
	"Automerge.h"
	"OpSet.h"
	"Change.h"
	"Change.cpp"
	"type.h"
	"ExId.h"
	"Op.h"
	"transaction/Transaction.h"
	"Patch.h"
	"Value.h"
	"IndexedCache.h"
	"OpTree.h"
	"Query.h"
	"helper.h"
	"query/OpId.h"
	"query/OpId.cpp"
	"OpTree.cpp"
	"OpSet.cpp"
	"query/QueryKeys.h"
	"query/QueryKeys.cpp"
	"transaction/Transaction.cpp"
	"query/SeekOp.h"
	"query/SeekOp.cpp"
	"Query.cpp"
	"query/SeekOpWithPatch.h"
	"query/SeekOpWithPatch.cpp"
	"OpObserver.h"
	"Clock.h"
	"Error.h"
	"transaction/Transactable.h"
	"query/Nth.h"
	"query/Nth.cpp"
	"query/QueryProp.h"
	"query/QueryProp.cpp"
	"Automerge.cpp"
	"query/InsertNth.h"
	"query/InsertNth.cpp"
	"transaction/CommitOptions.h"
	"helper.cpp"
	"type.cpp"
	"Encoder.h"
	"Encoder.cpp" 
	"leb128.h"
	"Decoder.h"
	"Decoder.cpp"
	"Columnar.h"
	"Columnar.cpp"
	"legacy.h"
	"legacy.cpp"
	"query/QueryKeysAt.h"
	"query/QueryKeysAt.cpp"
	"Keys.h"
	"Keys.cpp"
	"query/Len.h"
	"query/Len.cpp"
	"Op.cpp"
)

target_include_directories(automerge PUBLIC
	${VGG_CONTRIB_JSON_INCLUDE}
	${VGG_CONTRIB_PICOSHA2_INCLUDE}
	${VGG_CONTRIB_SDEFL_INCLUDE}
)

add_executable (demo
	"demo.cpp"
)

target_link_libraries (demo PRIVATE
	automerge
)
