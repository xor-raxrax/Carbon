export module RiblixStructures;

import <string>;
import <vector>;
import <memory>;

export
{
	struct __declspec(novtable) Descriptor
	{
		void* vftable;
		const std::string* name;
		void* isReplicable;
		void* isOutdated;
		void* attributes;
	};

	struct __declspec(novtable) MemberDescriptor : public Descriptor
	{
		const std::string* category;
		struct ClassDescriptor* owner;
		void* _1;
		void* _2;
		void* TType;
		void* _3;
		void* _4;
		void* _5;
		void* _6;
		void* _7;
		void* _8;
	};

	struct __declspec(novtable) PropertyDescriptor : public MemberDescriptor
	{
		struct GetSet {
			void* vftable;
			void* get;
			void* _1;
			void* set;
			void* _2;
		} *getset;
	};

	struct __declspec(novtable) EventDescriptor : public MemberDescriptor
	{

	};

	struct __declspec(novtable) FunctionDescriptor : public MemberDescriptor
	{
		void* _1;
		void* _2;
		void** _vftable; // ? array of functions
		void* function; //?
	};

	struct __declspec(novtable) YieldFunctionDescriptor : public MemberDescriptor
	{
		void* _1;
		void* _2;
		void** _3; //?
		void* function; //?
	};

	struct __declspec(novtable) CallbackDescriptor : public MemberDescriptor
	{

	};

	template <typename Member>
	struct __declspec(novtable) MemberDescriptorContainer
	{
		std::vector<Member*> collection;
		char __pad[168 - sizeof(collection)];

		Member* getDescriptor(const char* name)
		{
			for (auto member : collection)
				if (*member->name == name)
					return member;
			return nullptr;
		}
	};

	struct __declspec(novtable) ClassDescriptor
		: public Descriptor
		, public MemberDescriptorContainer<PropertyDescriptor>
		, public MemberDescriptorContainer<EventDescriptor>
		, public MemberDescriptorContainer<FunctionDescriptor>
		, public MemberDescriptorContainer<YieldFunctionDescriptor>
		, public MemberDescriptorContainer<CallbackDescriptor>
	{

	};

	struct __declspec(novtable) Instance
	{
		/*  0*/ void* vftable;
		/*  8*/ std::weak_ptr<Instance> shared;
		/* 18*/ ClassDescriptor* classDescriptor;
		/* 20*/ int _1[10];
		/* 48*/ const char* name;
		/* 50*/ std::shared_ptr<std::vector<std::shared_ptr<Instance>>> children;
		/* 60*/ Instance* parent;
	};

	class Capabilities
	{
	public:
		enum CapabilityType : uint32_t {
			Restricted = 0,
			Plugin = 1ULL << 0,
			LocalUser = 1ULL << 1,
			WritePlayer = 1ULL << 2,
			RobloxScript = 1ULL << 3,
			RobloxEngine = 1ULL << 4,
			NotAccessible = 1ULL << 5,
			RunClientScript = 1ULL << 8,
			RunServerScript = 1ULL << 9,
			AccessOutsideWrite = 1ULL << 11,
			SpecialCapability = 1ULL << 15,
			AssetRequire = 1ULL << 16,
			LoadString = 1ULL << 17,
			ScriptGlobals = 1ULL << 18,
			CreateInstances = 1ULL << 19,
			Basic = 1ULL << 20,
			Audio = 1ULL << 21,
			DataStore = 1ULL << 22,
			Network = 1ULL << 23,
			Physics = 1ULL << 24,
			Dummy = 1ULL << 25, // use this one as our thread marking

			OurThread = Dummy,
		};

		void set(CapabilityType capability) { bitfield |= capability; }
		void clear(CapabilityType capability) { bitfield &= ~capability; }
		bool isSet(CapabilityType capability) const { return bitfield & capability; }

		uint32_t bitfield;
	};

	struct RobloxExtraSpace
	{
		// intrusive set stuff
		RobloxExtraSpace* _1;
		void* _2;
		RobloxExtraSpace* _3;
		void* _4;
		void* ref_count_shared;
		void* _5;
		int identity;
		int _6;
		void* _7;
		Instance* scriptContext;
		Capabilities capabilities;
		uint32_t _8;
		Instance* script;
		void* ref_count_script;
	};

	struct Context
	{
		int identity;
		void* _1;
		void* _2;
		struct lua_State* _3;
		Capabilities capabilities;
		void* capabilitiesGetter;
	};

}