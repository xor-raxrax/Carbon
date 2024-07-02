export module Luau;

//#pragma once
//#include "Lib.h"

import <cstdint>;
import <string>;
import <iostream>;
import <stdarg.h>;

export
{

const int LUA_IDSIZE = 256; // LUA_IDSIZE gives the maximum size for the description of the source
const int LUA_MINSTACK = 20; // LUA_MINSTACK is the guaranteed number of Lua stack slots available to a C function
const int LUAI_MAXCSTACK = 8000; // LUAI_MAXCSTACK limits the number of Lua stack slots that a C function can use
const int LUAI_MAXCALLS = 20000; // LUAI_MAXCALLS limits the number of nested calls
const int LUAI_MAXCCALLS = 200; // LUAI_MAXCCALLS is the maximum depth for nested C calls; this limit depends on native stack size
const int LUA_BUFFERSIZE = 512; // buffer size used for on-stack string operations; this limit depends on native stack size
const int LUA_UTAG_LIMIT = 128; // number of valid Lua userdata tags
const int LUA_LUTAG_LIMIT = 128; // number of valid Lua lightuserdata tags
const int LUA_SIZECLASSES = 40; // upper bound for number of size classes used by page allocator
const int LUA_MEMORY_CATEGORIES = 256; // available number of separate memory categories
const int LUA_MINSTRTABSIZE = 32; // minimum size for the string table (must be power of 2)
const int LUA_MAXCAPTURES = 32; // maximum number of captures supported by pattern matching
const int LUA_VECTOR_SIZE = 3; // must be 3 or 4

const int LUA_EXTRA_SIZE = (LUA_VECTOR_SIZE - 2);

const int LUA_REGISTRYINDEX = (-LUAI_MAXCSTACK - 2000);
const int LUA_ENVIRONINDEX = (-LUAI_MAXCSTACK - 2001);
const int LUA_GLOBALSINDEX = (-LUAI_MAXCSTACK - 2002);

const int LUA_MULTRET = -1;

enum lua_Type
{
	LUA_TNONE = -1,
	LUA_TNIL = 0,     // must be 0 due to lua_isnoneornil
	LUA_TBOOLEAN = 1, // must be 1 due to l_isfalse


	LUA_TLIGHTUSERDATA,
	LUA_TNUMBER,
	LUA_TVECTOR,

	LUA_TSTRING, // all types above this must be value types, all types below this must be GC types - see iscollectable


	LUA_TTABLE,
	LUA_TFUNCTION,
	LUA_TUSERDATA,
	LUA_TTHREAD,
	LUA_TBUFFER,

	// values below this line are used in GCObject tags but may never show up in TValue type tags
	LUA_TPROTO,
	LUA_TUPVAL,
	LUA_TDEADKEY,

	// the count of TValue type tags
	LUA_T_COUNT = LUA_TPROTO
};

enum TMS
{
	TM_INDEX,
	TM_NEWINDEX,
	TM_MODE,
	TM_NAMECALL,
	TM_CALL,
	TM_ITER,
	TM_LEN,

	TM_EQ, // last tag method with `fast' access


	TM_ADD,
	TM_SUB,
	TM_MUL,
	TM_DIV,
	TM_IDIV,
	TM_MOD,
	TM_POW,
	TM_UNM,


	TM_LT,
	TM_LE,
	TM_CONCAT,
	TM_TYPE,
	TM_METATABLE,

	TM_N // number of elements in the enum
};

using lua_Number = double;
using lua_Integer = int;
using lua_Unsigned = unsigned;

struct TString;
struct stringtable
{
	TString** hash;
	uint32_t nuse; // number of elements
	int size;
};

struct lua_State;
typedef int (*lua_CFunction)(lua_State* L);
typedef int (*lua_Continuation)(lua_State* L, int status);
typedef void* (*lua_Alloc)(void* ud, void* ptr, size_t osize, size_t nsize);

struct luaL_Reg
{
	const char* name;
	lua_CFunction func;
};

union L_Umaxalign
{
	double u;
	void* s;
	long l;
};

struct Proto;

struct lua_ExecutionCallbacks
{
	void* context;
	void (*close)(lua_State* L);                 // called when global VM state is closed
	void (*destroy)(lua_State* L, Proto* proto); // called when function is destroyed
	int (*enter)(lua_State* L, Proto* proto);    // called when function is about to start/resume (when execdata is present), return 0 to exit VM
	void (*disable)(lua_State* L, Proto* proto); // called when function has to be switched from native to bytecode in the debugger
	size_t(*getmemorysize)(lua_State* L, Proto* proto); // called to request the size of memory associated with native part of the Proto
};

struct lua_Debug
{
	const char* name;      // (n)
	const char* what;      // (s) `Lua', `C', `main', `tail'
	const char* source;    // (s)
	const char* short_src; // (s)
	int linedefined;       // (s)
	int currentline;       // (l)
	unsigned char nupvals; // (u) number of upvalues
	unsigned char nparams; // (a) number of parameters
	char isvararg;         // (a)
	void* userdata;        // only valid in luau_callhook

	char ssbuf[LUA_IDSIZE];
};

struct lua_Callbacks
{
	void* userdata; // arbitrary userdata pointer that is never overwritten by Luau

	void (*interrupt)(lua_State* L, int gc);  // gets called at safepoints (loop back edges, call/ret, gc) if set
	void (*panic)(lua_State* L, int errcode); // gets called when an unprotected error is raised (if longjmp is used)

	void (*userthread)(lua_State* LP, lua_State* L); // gets called when L is created (LP == parent) or destroyed (LP == NULL)
	int16_t(*useratom)(const char* s, size_t l);    // gets called when a string is created; returned atom can be retrieved via tostringatom

	void (*debugbreak)(lua_State* L, lua_Debug* ar);     // gets called when BREAK instruction is encountered
	void (*debugstep)(lua_State* L, lua_Debug* ar);      // gets called after each instruction in single step mode
	void (*debuginterrupt)(lua_State* L, lua_Debug* ar); // gets called when thread execution is interrupted by break in another thread
	void (*debugprotectederror)(lua_State* L);           // gets called when protected call results in an error
};

struct GCStats
{
	// data for proportional-integral controller of heap trigger value
	int32_t triggerterms[32] = { 0 };
	uint32_t triggertermpos = 0;
	int32_t triggerintegral = 0;

	size_t atomicstarttotalsizebytes = 0;
	size_t endtotalsizebytes = 0;
	size_t heapgoalsizebytes = 0;

	double starttimestamp = 0;
	double atomicstarttimestamp = 0;
	double endtimestamp = 0;
};

#define CommonHeader \
		uint8_t tt; uint8_t marked; uint8_t memcat

struct GCheader
{
	CommonHeader;
};

union GCObject;

union Value
{
	GCObject* gc;
	void* p;
	double n;
	int b;
	float v[2]; // v[0], v[1] live here; v[2] lives in TValue::extra
};

struct TValue
{
	Value value;
	int extra[LUA_EXTRA_SIZE];
	int tt;
};

struct UpVal
{
	CommonHeader;
	uint8_t markedopen; // set if reachable from an alive thread (only valid during atomic)

	// 4 byte padding (x64)

	TValue* v; // points to stack or to its own value
	union
	{
		TValue value; // the value (when closed)
		struct
		{
			// global double linked list (when open)
			struct UpVal* prev;
			struct UpVal* next;

			// thread linked list (when open)
			struct UpVal* threadnext;
		} open;
	} u;

	static bool isopen(UpVal* up)
	{
		return (up)->v != &(up)->u.value;
	}
};

struct global_State
{
	stringtable strt; // hash table for strings

	lua_Alloc frealloc;   // function to reallocate memory
	void* ud;            // auxiliary data to `frealloc'

	uint8_t currentwhite;
	uint8_t gcstate; // state of garbage collector

	GCObject* gray;      // list of gray objects
	GCObject* grayagain; // list of objects to be traversed atomically
	GCObject* weak;     // list of weak tables (to be cleared)

	size_t GCthreshold;                       // when totalbytes > GCthreshold, run GC step
	size_t totalbytes;                        // number of bytes currently allocated
	int gcgoal;                               // see LUAI_GCGOAL
	int gcstepmul;                            // see LUAI_GCSTEPMUL
	int gcstepsize;                          // see LUAI_GCSTEPSIZE

	struct lua_Page* freepages[LUA_SIZECLASSES]; // free page linked list for each size class for non-collectable objects
	struct lua_Page* freegcopages[LUA_SIZECLASSES]; // free page linked list for each size class for collectable objects
	struct lua_Page* allpages; // page linked list with all pages for all non-collectable object classes (available with LUAU_ASSERTENABLED)
	struct lua_Page* allgcopages; // page linked list with all pages for all classes
	struct lua_Page* sweepgcopage; // position of the sweep in `allgcopages'

	size_t memcatbytes[LUA_MEMORY_CATEGORIES]; // total amount of memory used by each memory category

	struct lua_State* mainthread;
	UpVal uvhead;                                    // head of double-linked list of all open upvalues
	struct Table* mt[LUA_T_COUNT];                   // metatables for basic types
	TString* ttname[LUA_T_COUNT];       // names for basic types
	TString* tmname[TM_N];             // array with tag-method names

	TValue pseudotemp; // storage for temporary values used in pseudo2addr

	TValue registry; // registry table, used by lua_ref and LUA_REGISTRYINDEX
	int registryfree; // next free slot in registry

	struct lua_jmpbuf* errorjmp; // jump buffer data for longjmp-style error handling

	uint64_t rngstate; // PCG random number generator state
	uint64_t ptrenckey[4]; // pointer encoding key for display

	lua_Callbacks cb;
	lua_ExecutionCallbacks ecb;

	void (*udatagc[LUA_UTAG_LIMIT])(lua_State*, void*); // for each userdata tag, a gc callback to be called immediately before freeing memory

	TString* lightuserdataname[LUA_LUTAG_LIMIT]; // names for tagged lightuserdata

	GCStats gcstats;

#ifdef LUAI_GCMETRICS
	GCMetrics gcmetrics;
#endif
};
// clang-format on

using StkId = TValue*; // index to stack elements
using Instruction = uint32_t;

struct CallInfo
{
	StkId base;    // base for this function
	StkId func;    // function index in the stack
	StkId top;     // top for this function
	const Instruction* savedpc;

	int nresults;       // expected number of results from this function
	unsigned int flags; // call frame flags, see LUA_CALLINFO_*
};

/*
** `per thread' state
*/
// clang-format off
struct lua_State
{
	CommonHeader;
	uint8_t status;

	uint8_t activememcat; // memory category that is used for new GC object allocations

	bool isactive;   // thread is currently executing, stack may be mutated without barriers
	bool singlestep; // call debugstep hook after each instruction

	StkId top;                                        // first free slot in the stack
	StkId base;                                       // base of current function
	global_State* global;
	CallInfo* ci;                                     // call info for current function
	StkId stack_last;                                 // last free slot in the stack
	StkId stack;                                     // stack base

	CallInfo* end_ci;                          // points after end of ci array
	CallInfo* base_ci;                        // array of CallInfo's

	int stacksize;
	int size_ci;                              // size of array `base_ci'

	unsigned short nCcalls;     // number of nested C calls
	unsigned short baseCcalls; // nested C calls when resuming coroutine

	int cachedslot;    // when table operations or INDEX/NEWINDEX is invoked from Luau, what is the expected slot for lookup?

	Table* gt;           // table of globals
	UpVal* openupval;    // list of open upvalues in this stack
	GCObject* gclist;

	TString* namecall; // when invoked from Luau using NAMECALL, what method do we need to invoke?

	struct RobloxExtraSpace* userdata;
};

struct TString
{
	CommonHeader;

	// 1 byte padding
	int16_t atom;

	// 2 byte padding
	TString* next; // next string in the hash table bucket

	unsigned int hash;
	unsigned int len;

	char data[1]; // string data is allocated right after the header
};


typedef struct Udata
{
	CommonHeader;

	uint8_t tag;
	int len;
	struct Table* metatable;

	union
	{
		char data[1];      // userdata is allocated right after the header
		L_Umaxalign dummy; // ensures maximum alignment for data
	};
} Udata;

typedef struct Buffer
{
	CommonHeader;

	unsigned int len;
	union
	{
		char data[1];      // buffer is allocated right after the header
		L_Umaxalign dummy; // ensures maximum alignment for data
	};
} Buffer;

struct Proto
{
	CommonHeader;

	uint8_t nups; // number of upvalues
	uint8_t numparams;
	uint8_t is_vararg;
	uint8_t maxstacksize;
	uint8_t flags;


	TValue* k;              // constants used by the function
	Instruction* code;      // function bytecode
	struct Proto** p;       // functions defined inside the function
	const Instruction* codeentry;

	void* execdata;
	uintptr_t exectarget;


	uint8_t* lineinfo;      // for each instruction, line number as a delta from baseline
	int* abslineinfo;       // baseline line info, one entry for each 1<<linegaplog2 instructions; allocated after lineinfo
	struct LocVar* locvars; // information about local variables
	TString** upvalues;     // upvalue names
	TString* source;

	TString* debugname;
	uint8_t* debuginsn; // a copy of code[] array with just opcodes

	uint8_t* typeinfo;

	void* userdata;

	GCObject* gclist;


	int sizecode;
	int sizep;
	int sizelocvars;
	int sizeupvalues;
	int sizek;
	int sizelineinfo;
	int linegaplog2;
	int linedefined;
	int bytecodeid;
	int sizetypeinfo;
};

// clang-format on

struct LocVar
{
	TString* varname;
	int startpc; // first point where variable is active
	int endpc;   // first point where variable is dead
	uint8_t reg; // register slot, relative to base, where variable is stored
};

struct Closure
{
	CommonHeader;

	uint8_t isC;
	uint8_t nupvalues;
	uint8_t stacksize;
	uint8_t preload;

	GCObject* gclist;
	struct Table* env;

	union
	{
		struct
		{
			lua_CFunction f;
			lua_Continuation cont;
			const char* debugname;
			TValue upvals[1];
		} c;

		struct
		{
			Proto* p;
			TValue uprefs[1];
		} l;
	};
};

struct TKey
{
	::Value value;
	int extra[LUA_EXTRA_SIZE];
	unsigned tt : 4;
	int next : 28; // for chaining
};

struct LuaNode
{
	TValue val;
	TKey key;
};

struct Table
{
	CommonHeader;

	uint8_t tmcache;    // 1<<p means tagmethod(p) is not present
	uint8_t readonly;   // sandboxing feature to prohibit writes to table
	uint8_t safeenv;    // environment doesn't share globals with other scripts
	uint8_t lsizenode;  // log2 of size of `node' array
	uint8_t nodemask8; // (1<<lsizenode)-1, truncated to 8 bits

	int sizearray; // size of `array' array
	union
	{
		int lastfree;  // any free position is before this position
		int aboundary; // negated 'boundary' of `array' array; iff aboundary < 0
	};

	struct Table* metatable;
	TValue* array;  // array part
	LuaNode* node;
	GCObject* gclist;
};

union GCObject
{
	GCheader gch;
	TString ts;
	Udata u;
	Closure cl;
	Table h;
	Proto p;
	UpVal uv;
	lua_State th; // thread
	Buffer buf;
};


struct LuaApiAddresses
{
	// TODO: unimplemented loadfile
	//int (*lua_pcall)(lua_State* L, int nargs, int nresults, int errfunc);

	TValue* luaO_nilobject = nullptr;

	int (*lua_getinfo)(lua_State* L, int level, const char* what, lua_Debug* ar) = nullptr;

	void (*luaL_typeerrorL)(lua_State* L, int narg, const char* tname) = nullptr;
	void (*luaL_errorL)(lua_State* L, const char* fmt, ...) = nullptr;
	const char* (*luaL_typename)(lua_State* L, int idx) = nullptr;

	void (*lua_pushlstring)(lua_State*, const char* s, size_t len) = nullptr;
	void (*lua_pushvalue)(lua_State*, int idx) = nullptr;
	const char* (*lua_tolstring)(lua_State*, int, size_t*) = nullptr;

	void (*lua_settable)(lua_State*, int idx) = nullptr;
	int (*lua_getfield)(lua_State* L, int idx, const char* k) = nullptr;
	void (*lua_setfield)(lua_State* L, int idx, const char* k) = nullptr;

	void (*lua_createtable)(lua_State*, int narr, int nrec) = nullptr;
	Table* (*luaH_clone)(lua_State* L, Table* tt) = nullptr;
	int (*lua_next)(lua_State* L, int idx) = nullptr;
	int (*lua_rawget)(lua_State* L, int idx) = nullptr;
	void (*lua_rawset)(lua_State* L, int idx) = nullptr;
	int (*lua_setmetatable)(lua_State*, int objindex) = nullptr;
	int (*lua_getmetatable)(lua_State*, int objindex) = nullptr;

	void (*lua_pushcclosurek)(lua_State* L, lua_CFunction fn, const char* debugname, int nup, lua_Continuation cont) = nullptr;
	Closure* (*luaF_newLclosure)(lua_State* L, int nelems, Table* e, Proto* p) = nullptr;

	void (*luaD_call)(lua_State* L, StkId func, int nresults) = nullptr;

	Proto* (*luaF_newproto)(lua_State* L) = nullptr;

	void (*luaD_reallocCI)(lua_State* L, int newsize) = nullptr;
	CallInfo* (*luaD_growCI)(lua_State* L) = nullptr;

	void (*lua_concat)(lua_State* L, int n) = nullptr;
	void* (*lua_newuserdatatagged)(lua_State* L, size_t sz, int tag) = nullptr;
	const TValue* (*luaH_get)(Table* t, const TValue* key) = nullptr;
};

LuaApiAddresses luaApiAddresses;

TValue* luaO_nilobject() {
	return luaApiAddresses.luaO_nilobject;
}

int lua_getinfo(lua_State* L, int level, const char* what, lua_Debug* ar) {
	return luaApiAddresses.lua_getinfo(L, level, what, ar);
}

[[noreturn]] void luaL_typeerrorL(lua_State* L, int narg, const char* tname) {
	luaApiAddresses.luaL_typeerrorL(L, narg, tname);
}

const char* luaL_typename(lua_State* L, int idx) {
	return luaApiAddresses.luaL_typename(L, idx);
}

void lua_pushlstring(lua_State* L, const char* s, size_t len) {
	luaApiAddresses.lua_pushlstring(L, s, len);
}

void lua_pushvalue(lua_State* L, int idx) {
	luaApiAddresses.lua_pushvalue(L, idx);
}

const char* lua_tolstring(lua_State* L, int idx, size_t* len) {
	return luaApiAddresses.lua_tolstring(L, idx, len);
}

void lua_settable(lua_State* L, int idx) {
	luaApiAddresses.lua_settable(L, idx);
}

int lua_getfield(lua_State* L, int idx, const char* k) {
	return luaApiAddresses.lua_getfield(L, idx, k);
}

void lua_setfield(lua_State* L, int idx, const char* k) {
	luaApiAddresses.lua_setfield(L, idx, k);
}

void lua_createtable(lua_State* L, int narr = 0, int nrec = 0) {
	luaApiAddresses.lua_createtable(L, narr, nrec);
}

Table* luaH_clone(lua_State* L, Table* tt) {
	return luaApiAddresses.luaH_clone(L, tt);
}

int lua_next(lua_State* L, int idx) {
	return luaApiAddresses.lua_next(L, idx);
}

int lua_rawget(lua_State* L, int idx) {
	return luaApiAddresses.lua_rawget(L, idx);
}

void lua_rawset(lua_State* L, int idx) {
	luaApiAddresses.lua_rawset(L, idx);
}

int lua_setmetatable(lua_State* L, int objindex) {
	return luaApiAddresses.lua_setmetatable(L, objindex);
}

int lua_getmetatable(lua_State* L, int objindex) {
	return luaApiAddresses.lua_getmetatable(L, objindex);
}

void lua_pushcclosurek(lua_State* L, lua_CFunction fn, const char* debugname, int nup, lua_Continuation cont) {
	luaApiAddresses.lua_pushcclosurek(L, fn, debugname, nup, cont);
}

Closure* luaF_newLclosure(lua_State* L, int nelems, Table* e, Proto* p) {
	return luaApiAddresses.luaF_newLclosure(L, nelems, e, p);
}

void luaD_call(lua_State* L, StkId func, int nresults) {
	luaApiAddresses.luaD_call(L, func, nresults);
}

Proto* luaF_newproto(lua_State* L) {
	return luaApiAddresses.luaF_newproto(L);
}

void* lua_newuserdatatagged(lua_State* L, size_t sz, int tag = 0) {
	return luaApiAddresses.lua_newuserdatatagged(L, sz, tag);
}

const TValue* luaH_get(Table* t, const TValue* key) {
	return luaApiAddresses.luaH_get(t, key);
}

int lua_type(lua_State* L, int idx);

template <typename T>
concept GCObjectOrTValue = std::same_as<T, GCObject> || std::same_as<T, TValue>;

template <GCObjectOrTValue T> lua_Type ttype(const T* o) { return (lua_Type)o->tt; }

template <GCObjectOrTValue T> bool ttisnil(const T* o) { return ttype(o) == LUA_TNIL; }
template <GCObjectOrTValue T> bool ttisnumber(const T* o) { return ttype(o) == LUA_TNUMBER; }
template <GCObjectOrTValue T> bool ttisstring(const T* o) { return ttype(o) == LUA_TSTRING; }
template <GCObjectOrTValue T> bool ttistable(const T* o) { return ttype(o) == LUA_TTABLE; }
template <GCObjectOrTValue T> bool ttisfunction(const T* o) { return ttype(o) == LUA_TFUNCTION; }
template <GCObjectOrTValue T> bool ttisboolean(const T* o) { return ttype(o) == LUA_TBOOLEAN; }
template <GCObjectOrTValue T> bool ttisuserdata(const T* o) { return ttype(o) == LUA_TUSERDATA; }
template <GCObjectOrTValue T> bool ttisthread(const T* o) { return ttype(o) == LUA_TTHREAD; }
template <GCObjectOrTValue T> bool ttisbuffer(const T* o) { return ttype(o) == LUA_TBUFFER; }
template <GCObjectOrTValue T> bool ttislightuserdata(const T* o) { return ttype(o) == LUA_TLIGHTUSERDATA; }
template <GCObjectOrTValue T> bool ttisvector(const T* o) { return ttype(o) == LUA_TVECTOR; }
template <GCObjectOrTValue T> bool ttisupval(const T* o) { return ttype(o) == LUA_TUPVAL; }

template <GCObjectOrTValue T> GCObject* gcvalue(const T* o) { return o->value.gc; }
template <GCObjectOrTValue T> void* pvalue(const T* o) { return o->value.p; }
template <GCObjectOrTValue T> lua_Number nvalue(const T* o) { return o->value.n; }
template <GCObjectOrTValue T> float* vvalue(const T* o) { return const_cast<float*>(o->value.v); }
template <GCObjectOrTValue T> TString* tsvalue(const T* o) { return &(o->value.gc->ts); }
template <GCObjectOrTValue T> Udata* uvalue(const T* o) { return &(o->value.gc->u); }
template <GCObjectOrTValue T> Closure* clvalue(const T* o) { return &(o->value.gc->cl); }
template <GCObjectOrTValue T> Table* hvalue(const T* o) { return &(o->value.gc->h); }
template <GCObjectOrTValue T> int bvalue(const T* o) { return o->value.b; }
template <GCObjectOrTValue T> lua_State* thvalue(const T* o) { return &(o->value.gc->th); }
template <GCObjectOrTValue T> Buffer* bufvalue(const T* o) { return &(o->value.gc->buf); }
template <GCObjectOrTValue T> UpVal* upvalue(const T* o) { return &(o->value.gc->uv); }

const char* getstr(const TString* s) { return s->data; }
const char* svalue(const TValue* o) { return getstr(tsvalue(o)); }


bool lua_isfunction(lua_State* L, int n) { return lua_type(L, (n)) == LUA_TFUNCTION; }
bool lua_istable(lua_State* L, int n) { return lua_type(L, (n)) == LUA_TTABLE; }
bool lua_islightuserdata(lua_State* L, int n) { return lua_type(L, (n)) == LUA_TLIGHTUSERDATA; }
bool lua_isnil(lua_State* L, int n) { return lua_type(L, (n)) == LUA_TNIL; }
bool lua_isboolean(lua_State* L, int n) { return lua_type(L, (n)) == LUA_TBOOLEAN; }
bool lua_isvector(lua_State* L, int n) { return lua_type(L, (n)) == LUA_TVECTOR; }
bool lua_isthread(lua_State* L, int n) { return lua_type(L, (n)) == LUA_TTHREAD; }
bool lua_isbuffer(lua_State* L, int n) { return lua_type(L, (n)) == LUA_TBUFFER; }
bool lua_isnone(lua_State* L, int n) { return lua_type(L, (n)) == LUA_TNONE; }
bool lua_isnoneornil(lua_State* L, int n) { return lua_type(L, (n)) <= LUA_TNIL; }

bool iscfunction(const TValue* o) { return ttype(o) == LUA_TFUNCTION && clvalue(o)->isC; }
bool isluafunction(const TValue* o) { return ttype(o) == LUA_TFUNCTION && !clvalue(o)->isC; }

bool iscfunction(const Closure* o) { return o->isC; }
bool isluafunction(const Closure* o) { return !o->isC; }

Closure* curr_func(lua_State* L) { return clvalue(L->ci->func); }

void luaL_errorL(lua_State* L, const char* fmt, ...);

TValue* getupvalue(Closure* function, int index)
{
	if (function->isC)
	{
		return &function->c.upvals[index];
	}
	else
	{
		TValue* up = &function->l.uprefs[index];
		return ttisupval(up) ? upvalue(up)->v : up;
	}
}

int lua_upvalueindex(int i)
{
	return LUA_GLOBALSINDEX - i;
}

bool luai_veceq(const float* a, const float* b)
{
#if LUA_VECTOR_SIZE == 4
	return a[0] == b[0] && a[1] == b[1] && a[2] == b[2] && a[3] == b[3];
#else
	return a[0] == b[0] && a[1] == b[1] && a[2] == b[2];
#endif
}

const char* currfuncname(lua_State* L)
{
	Closure* cl = L->ci > L->base_ci ? curr_func(L) : nullptr;
	const char* debugname = cl && cl->isC ? cl->c.debugname + 0 : nullptr;

	if (debugname && strcmp(debugname, "__namecall") == 0)
		return L->namecall ? getstr(L->namecall) : nullptr;
	else
		return debugname;
}

[[noreturn]] void luaL_argerrorL(lua_State* L, int narg, const char* extramsg)
{
	const char* fname = currfuncname(L);

	if (fname)
		luaL_errorL(L, "invalid argument #%d to '%s' (%s)", narg, fname, extramsg);
	else
		luaL_errorL(L, "invalid argument #%d (%s)", narg, extramsg);
}


Table* getcurrenv(lua_State* L)
{
	if (L->ci == L->base_ci) // no enclosing function?
		return L->gt;        // use global table as environment
	else
		return curr_func(L)->env;
}

void sethvalue(TValue* obj, Table* x);

TValue* pseudo2addr(lua_State* L, int idx)
{
	switch (idx)
	{ // pseudo-indices
	case LUA_REGISTRYINDEX:
		return &L->global->registry;
	case LUA_ENVIRONINDEX:
	{
		sethvalue(&L->global->pseudotemp, getcurrenv(L));
		return &L->global->pseudotemp;
	}
	case LUA_GLOBALSINDEX:
	{
		sethvalue(&L->global->pseudotemp, L->gt);
		return &L->global->pseudotemp;
	}
	default:
	{
		Closure* func = curr_func(L);
		idx = LUA_GLOBALSINDEX - idx;
		return (idx <= func->nupvalues) ? &func->c.upvals[idx - 1] : (TValue*)&luaO_nilobject;
	}
	}
}

TValue* index2addr(lua_State* L, int idx)
{
	if (idx > 0)
	{
		TValue* o = L->base + (idx - 1);
		if (o >= L->top)
			return luaO_nilobject();
		else
			return o;
	}
	else if (idx > LUA_REGISTRYINDEX)
	{
		return L->top + idx;
	}
	else
	{
		return pseudo2addr(L, idx);
	}
}

const char* const luaT_typenames[] = {
	// ORDER TYPE
	"nil",
	"boolean",

	"userdata",
	"number",
	"vector",

	"string",

	"table",
	"function",
	"userdata",
	"thread",
	"buffer",
};

const char* lua_typename(lua_State* L, int t)
{
	return (t == LUA_TNONE) ? "no value" : luaT_typenames[t];
}

void tag_error(lua_State* L, int narg, int tag)
{
	luaL_typeerrorL(L, narg, lua_typename(L, tag));
}

int lua_type(lua_State* L, int idx)
{
	StkId o = index2addr(L, idx);
	return (o == luaO_nilobject()) ? LUA_TNONE : ttype(o);
}

void luaL_checktype(lua_State* L, int narg, lua_Type t)
{
	if (lua_type(L, narg) != t)
		tag_error(L, narg, t);
}

void luaL_checkany(lua_State* L, int narg)
{
	if (lua_type(L, narg) == LUA_TNONE)
		luaL_errorL(L, "missing argument #%d", narg);
}

void lua_setglobal(lua_State* L, const char* name)
{
	lua_setfield(L, LUA_GLOBALSINDEX, name);
}

void lua_getglobal(lua_State* L, const char* name)
{
	lua_getfield(L, LUA_GLOBALSINDEX, name);
}

#define cast_byte(i) (uint8_t(i))
#define cast_num(i) (double(i))
#define cast_int(i) (int(i))

void setnilvalue(TValue* obj)
{
	obj->tt = LUA_TNIL;
}

void setbvalue(TValue* obj, bool x)
{
	obj->value.b = x;
	obj->tt = LUA_TBOOLEAN;
}

void setpvalue(TValue* obj, void* x, int tag)
{
	obj->value.p = x;
	obj->extra[0] = tag;
	obj->tt = LUA_TLIGHTUSERDATA;
}

void setnvalue(TValue* obj, double x)
{
	obj->value.n = x;
	obj->tt = LUA_TNUMBER;
}

void setsvalue(TValue* obj, TString* x)
{
	obj->value.gc = (GCObject*)x;
	obj->tt = LUA_TSTRING;
}

void setuvalue(TValue* obj, UpVal* x)
{
	obj->value.gc = (GCObject*)x;
	obj->tt = LUA_TUSERDATA;
}

void setvvalue(TValue* obj, float x, float y, float z)
{
	float* vec = obj->value.v;
	vec[0] = (x);
	vec[1] = (y);
	vec[2] = (z);
	obj->tt = LUA_TVECTOR;
}

void sethvalue(TValue* obj, Table* x)
{
	obj->value.gc = (GCObject*)x;
	obj->tt = LUA_TTABLE;
}

void setclvalue(TValue* obj, Closure* x)
{
	obj->value.gc = (GCObject*)x;
	obj->tt = LUA_TFUNCTION;
}

void setuvalue(TValue* obj, Udata* x)
{
	obj->value.gc = (GCObject*)x;
	obj->tt = LUA_TUSERDATA;
}

void setthvalue(TValue* obj, lua_State* x)
{
	obj->value.gc = (GCObject*)x;
	obj->tt = LUA_TTHREAD;
}

void setbufvalue(TValue* obj, Buffer* x)
{
	obj->value.gc = (GCObject*)x;
	obj->tt = LUA_TBUFFER;
}

void setobj(TValue* obj1, const TValue* obj2)
{
	*obj1 = *obj2;
}

void setobjfull(TValue* obj1, const TValue* obj2)
{
	*obj1 = *obj2;
	obj1->tt = obj2->tt;
}


void lua_settop(lua_State* L, int idx)
{
	if (idx >= 0)
	{
		while (L->top < L->base + idx)
			setnilvalue(L->top++);
		L->top = L->base + idx;
	}
	else
	{
		L->top += idx + 1; // `subtract' index (index is negative)
	}
}

void lua_pop(lua_State* L, int n)
{
	lua_settop(L, -(n)-1);
}

void incr_top(lua_State* L)
{
	L->top++;
}

int luaO_str2d(const char* s, double* result)
{
	char* endptr;
	*result = strtod(s, &endptr);
	if (endptr == s)
		return 0;                         // conversion failed
	if (*endptr == 'x' || *endptr == 'X') // maybe an hexadecimal constant?
		*result = cast_num(strtoul(s, &endptr, 16));
	if (*endptr == '\0')
		return 1; // most common case
	while (isspace(unsigned char(*endptr)))
		endptr++;
	if (*endptr != '\0')
		return 0; // invalid trailing characters?
	return 1;
}

const TValue* luaV_tonumber(const TValue* obj, TValue* n)
{
	double num;
	if (ttisnumber(obj))
		return obj;
	if (ttisstring(obj) && luaO_str2d(svalue(obj), &num))
	{
		setnvalue(n, num);
		return n;
	}
	else
		return nullptr;
}

#define tonumber(o, n) (ttype(o) == LUA_TNUMBER || (((o) = luaV_tonumber(o, n)) != NULL))

double lua_tonumberx(lua_State* L, int idx, bool* isnum = nullptr)
{
	TValue n;
	const TValue* o = index2addr(L, idx);
	if (tonumber(o, &n))
	{
		if (isnum)
			*isnum = 1;
		return nvalue(o);
	}
	else
	{
		if (isnum)
			*isnum = 0;
		return 0;
	}
}

int lua_tointegerx(lua_State* L, int idx, bool* isnum = nullptr)
{
	TValue n;
	const TValue* o = index2addr(L, idx);
	if (tonumber(o, &n))
	{
		int res;
		double num = nvalue(o);
		res = (int)num;
		if (isnum)
			*isnum = 1;
		return res;
	}
	else
	{
		if (isnum)
			*isnum = 0;
		return 0;
	}
}

unsigned lua_tounsignedx(lua_State* L, int idx, bool* isnum = nullptr)
{
	TValue n;
	const TValue* o = index2addr(L, idx);
	if (tonumber(o, &n))
	{
		unsigned res;
		double num = nvalue(o);
		res = (unsigned)(long long)(num);
		if (isnum)
			*isnum = 1;
		return res;
	}
	else
	{
		if (isnum)
			*isnum = 0;
		return 0;
	}
}

const TValue* luaA_toobject(lua_State* L, int idx)
{
	StkId p = index2addr(L, idx);
	return (p == luaO_nilobject()) ? nullptr : p;
}

void* lua_touserdata(lua_State* L, int idx)
{
	StkId o = index2addr(L, idx);
	if (ttisuserdata(o))
		return uvalue(o)->data;
	else if (ttislightuserdata(o))
		return pvalue(o);
	else
		return nullptr;
}

lua_State* lua_tothread(lua_State* L, int idx)
{
	StkId o = index2addr(L, idx);
	return (!ttisthread(o)) ? nullptr : thvalue(o);
}

Table* lua_totable(lua_State* L, int idx)
{
	StkId o = index2addr(L, idx);
	return (!ttistable(o)) ? nullptr : hvalue(o);
}

Closure* lua_toclosure(lua_State* L, int idx)
{
	StkId o = index2addr(L, idx);
	return (!ttisfunction(o)) ? nullptr : clvalue(o);
}

TString* lua_torawstring(lua_State* L, int idx)
{
	StkId o = index2addr(L, idx);
	return (!ttisstring(o)) ? nullptr : tsvalue(o);
}

bool lua_toboolean(lua_State* L, int idx)
{
	const TValue* o = index2addr(L, idx);
	return !(ttisnil(o) || (ttisboolean(o) && o->value.b == 0));
}

void luaA_pushobject(lua_State* L, const TValue* o)
{
	setobj(L->top, o);
	incr_top(L);
}
void lua_pushnil(lua_State* L)
{
	setnilvalue(L->top);
	incr_top(L);
}

void lua_pushboolean(lua_State* L, int b)
{
	setbvalue(L->top, (b != 0)); // ensure that true is 1
	incr_top(L);
}

void lua_pushlightuserdatatagged(lua_State* L, void* p, int tag = 0)
{
	setpvalue(L->top, p, tag);
	incr_top(L);
}

void lua_pushnumber(lua_State* L, double n)
{
	setnvalue(L->top, n);
	incr_top(L);
}

void lua_pushinteger(lua_State* L, int n)
{
	setnvalue(L->top, cast_num(n));
	incr_top(L);
}

void lua_pushunsigned(lua_State* L, unsigned u)
{
	setnvalue(L->top, cast_num(u));
	incr_top(L);
}

void lua_pushvector(lua_State* L, float x, float y, float z)
{
	setvvalue(L->top, x, y, z);
	incr_top(L);
}

void lua_pushstring(lua_State* L, const char* s)
{
	if (s == nullptr)
		lua_pushnil(L);
	else
		lua_pushlstring(L, s, strlen(s));
}

void lua_pushrawstring(lua_State* L, TString* value)
{
	setsvalue(L->top, value);
	incr_top(L);
}

void lua_pushrawtable(lua_State* L, Table* value)
{
	sethvalue(L->top, value);
	incr_top(L);
}

void lua_pushclosure(lua_State* L, Closure* cl)
{
	setclvalue(L->top, cl);
	incr_top(L);
}

void lua_pushrawuserdata(lua_State* L, Udata* u)
{
	setuvalue(L->top, u);
	incr_top(L);
}

void lua_pushrawthread(lua_State* L, lua_State* thread)
{
	setthvalue(L->top, thread);
	incr_top(L);
}

void lua_pushrawbuffer(lua_State* L, Buffer* buffer)
{
	setbufvalue(L->top, buffer);
	incr_top(L);
}

void lua_pushrawgcobject(lua_State* L, GCObject* x)
{
	L->top->value.gc = (GCObject*)x;
	L->top->tt = x->gch.tt;
	incr_top(L);
}

Closure* luaL_checkfunction(lua_State* L, int narg)
{
	Closure* func = lua_toclosure(L, narg);
	if (!func)
		tag_error(L, narg, LUA_TFUNCTION);
	return func;
}

Table* luaL_checktable(lua_State* L, int narg)
{
	Table* table = lua_totable(L, narg);
	if (!table)
		tag_error(L, narg, LUA_TTABLE);
	return table;
}

int luaL_checkinteger(lua_State* L, int narg)
{
	bool isnum;
	int d = lua_tointegerx(L, narg, &isnum);
	if (!isnum)
		tag_error(L, narg, LUA_TNUMBER);
	return d;
}

bool luaL_checkboolean(lua_State* L, int narg) {
	// This checks specifically for boolean values, ignoring
	// all other truthy/falsy values. If the desired result
	// is true if value is present then lua_toboolean should
	// directly be used instead.
	if (!lua_isboolean(L, narg))
		tag_error(L, narg, LUA_TBOOLEAN);
	return lua_toboolean(L, narg);
}

bool luaL_optboolean(lua_State* L, int narg, bool def)
{
	if (lua_isnoneornil(L, narg))
		return def;
	return luaL_checkboolean(L, narg);
}

const char* luaL_checklstring(lua_State* L, int narg, size_t* len = nullptr)
{
	const char* s = lua_tolstring(L, narg, len);
	if (!s)
		tag_error(L, narg, LUA_TSTRING);
	return s;
}

const char* luaL_optlstring(lua_State* L, int narg, const char* def, size_t* len = nullptr)
{
	if (lua_isnoneornil(L, narg))
	{
		if (len)
			*len = (def ? strlen(def) : 0);
		return def;
	}
	else
		return luaL_checklstring(L, narg, len);
}

bool lua_iscfunction(lua_State* L, int idx)
{
	StkId o = index2addr(L, idx);
	return ttype(o) == LUA_TFUNCTION && clvalue(o)->isC;
}

bool lua_isluafunction(lua_State* L, int idx)
{
	StkId o = index2addr(L, idx);
	return ttype(o) == LUA_TFUNCTION && !clvalue(o)->isC;
}

bool lua_isnumber(lua_State* L, int idx)
{
	TValue n;
	const TValue* o = index2addr(L, idx);
	return tonumber(o, &n);
}

bool lua_isstring(lua_State* L, int idx)
{
	int t = lua_type(L, idx);
	return t == LUA_TSTRING || t == LUA_TNUMBER;
}

void lua_pushcclosure(lua_State* L, lua_CFunction fn, const char* debugname = nullptr, int nup = 0)
{
	lua_pushcclosurek(L, fn, debugname, nup, nullptr);
}

int lua_gettop(lua_State* L)
{
	return cast_int(L->top - L->base);
}

void adjustresults(lua_State* L, int nres)
{
	if (nres == LUA_MULTRET && L->top >= L->ci->top)
		L->ci->top = L->top;
}

void lua_call(lua_State* L, int nargs, int nresults)
{
	StkId func;
	func = L->top - (nargs + 1);
	luaD_call(L, func, nresults);
	adjustresults(L, nresults);
}

int luaL_optinteger(lua_State* L, int narg, int def)
{
	if (lua_isnoneornil(L, narg))
		return def;

	return luaL_checkinteger(L, narg);
}


CallInfo* getcallinfo(lua_State* L, int level)
{
	if (unsigned(level) < unsigned(L->ci - L->base_ci))
		return L->ci - level;

	return nullptr;
}


CallInfo* getcallinfo(lua_State* L, int idx, bool opt)
{
	int level = opt ? luaL_optinteger(L, idx, 1) : luaL_checkinteger(L, idx);
	if (level < 0)
		luaL_argerrorL(L, idx, "level must be non-negative");

	return getcallinfo(L, level);
}

Closure* getclosure(lua_State* L, int idx, bool opt)
{
	StkId o = index2addr(L, idx);
	if (ttype(o) == LUA_TFUNCTION)
		return clvalue(o);

	if (auto callInfo = getcallinfo(L, idx, opt))
		return clvalue(callInfo->func);
	
	luaL_argerrorL(L, idx, "invalid level");
}

lua_State* getthread(lua_State* L, int& argsBase)
{
	if (lua_isthread(L, 1))
	{
		argsBase = 1;
		return lua_tothread(L, 1);
	}
	else
	{
		argsBase = 0;
		return L;
	}
}

// thread status; 0 is OK
enum lua_Status
{
	LUA_OK = 0,
	LUA_YIELD,
	LUA_ERRRUN,
	LUA_ERRSYNTAX, // legacy error code, preserved for compatibility
	LUA_ERRMEM,
	LUA_ERRERR,
	LUA_BREAK, // yielded for a debug breakpoint
};

class lua_exception : public std::exception
{
public:
	lua_exception(lua_State* L, int status)
		: L(L)
		, status(status)
	{
	}

	const char* what() const throw() override
	{
		// LUA_ERRRUN passes error object on the stack
		if (status == LUA_ERRRUN)
			if (const char* str = lua_tolstring(L, -1, nullptr))
				return str;

		switch (status)
		{
		case LUA_ERRRUN:
			return "lua_exception: runtime error";
		case LUA_ERRSYNTAX:
			return "lua_exception: syntax error";
		case LUA_ERRMEM:
			return "lua_exception: " "not enough memory";
		case LUA_ERRERR:
			return "lua_exception: " "error in error handling";
		default:
			return "lua_exception: unexpected exception status";
		}
	}

	int getStatus() const
	{
		return status;
	}

	const lua_State* getThread() const
	{
		return L;
	}

private:
	lua_State* L;
	int status;
};

[[noreturn]] void luaD_throw(lua_State* L, int errcode)
{
	throw lua_exception(L, errcode);
}

#pragma warning( push )
#pragma warning( disable : 4996) // This function or variable may be unsafe...
const char* luaO_chunkid(char* buf, size_t buflen, const char* source, size_t srclen)
{
	if (*source == '=')
	{
		if (srclen <= buflen)
			return source + 1;
		// truncate the part after =
		memcpy(buf, source + 1, buflen - 1);
		buf[buflen - 1] = '\0';
	}
	else if (*source == '@')
	{
		if (srclen <= buflen)
			return source + 1;
		// truncate the part after @
		memcpy(buf, "...", 3);
		memcpy(buf + 3, source + srclen - (buflen - 4), buflen - 4);
		buf[buflen - 1] = '\0';
	}
	else
	{                                         // buf = [string "string"]
		size_t len = strcspn(source, "\n\r"); // stop at first newline
		buflen -= sizeof("[string \"...\"]");
		if (len > buflen)
			len = buflen;
		strcpy(buf, "[string \"");
		if (source[len] != '\0')
		{ // must truncate?
			strncat(buf, source, len);
			strcat(buf, "...");
		}
		else
			strcat(buf, source);
		strcat(buf, "\"]");
	}
	return buf;
}
#pragma warning( pop ) 

int getcurrentline(const Closure* func, const CallInfo* call)
{
	if (func->isC)
		return -1;

	if (call)
	{
		auto p = clvalue(call->func)->l.p;
		if (!p->lineinfo)
		{
			return 0;
		}
		else
		{
			int pc = call->savedpc ? int(call->savedpc - p->code) - 1 : 0;
			return p->abslineinfo[pc >> p->linegaplog2] + p->lineinfo[pc];
		}
	}
	else
	{
		return func->l.p->linedefined;
	}
}

void luaL_where(lua_State* L, int level)
{
	auto ci = getcallinfo(L, level);
	auto func = clvalue(ci->func);

	if (func->isC)
	{
		lua_pushstring(L, "");
		return;
	}

	char ssbuf[LUA_IDSIZE];
	TString* source = func->l.p->source;
	auto short_src = luaO_chunkid(ssbuf, sizeof(ssbuf), getstr(source), source->len);
	char result[LUA_BUFFERSIZE];
	snprintf(result, LUA_BUFFERSIZE, "%s:%d: ", short_src, getcurrentline(func, ci));
	lua_pushstring(L, result);
}

void luaL_errorL(lua_State* L, const char* fmt, ...)
{
	luaL_where(L, 1);

	va_list args;
	va_start(args, fmt);
	char result[LUA_BUFFERSIZE];
	vsnprintf(result, sizeof(result), fmt, args);
	va_end(args);
	lua_pushstring(L, result);

	luaApiAddresses.lua_concat(L, 2);
	luaD_throw(L, LUA_ERRRUN);
}

} // export