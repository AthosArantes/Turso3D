// MIT License
//
// Copyright(c) 2019 Samuel Kahn
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files(the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions :
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#pragma once

#include <cstdint>
#include <cstring>
#include <memory>
#include <type_traits>

#include "internal/Platform.hpp"
#include "internal/Utils_Preprocessor.hpp"

// Minimalistic and efficient RTTI Implementation.
// Adds virtual methods to registered types, as polymorphism is necessary
// Dynamic casts cost in the worst case one virtual call and walking through a data buffer

// Note:
// * This is not safe to pass across boundaries.
// * This is not thread safe. The type info is created the first time it is accessed, race conditions may occur.

/*Usage :

//For all polymorphic types including base classes
class Type : public Base1, public Base2
{
public:
	RTTI_IMPL()
	//...
};

//For all types except types using inheritance
RTTI_REGISTER(Type)
//For types using inheritance
RTTI_REGISTER(Type, Base1, Base2 ...)
*/

// Details, this is not meant to be used outside of this file
namespace RTTI_Private
{
	// You can reduce the size at will here
	typedef uint64_t typeid_t;

	// Member ::Get() will return const TypeInfo*
	template<typename T>
	struct GetTypeInfo
	{
	};

#ifdef _M_X64
	// Hash a string using FNV1a algorithm.
	constexpr uint64_t HashString(const char* const str, const uint64_t value = 0xCBF29CE484222325) noexcept
	{
		constexpr uint64_t prime = 0x100000001B3;
		return (str[0] == '\0') ? value : HashString(&str[1], (value ^ uint64_t(str[0])) * prime);
	}
#else
	// Hash a string using FNV1a algorithm.
	constexpr uint32_t HashString(const char* const str, const uint32_t value = 0x811C9DC5) noexcept
	{
		constexpr uint32_t prime = 0x1000193;
		return (str[0] == '\0') ? value : HashString(&str[1], (value ^ uint32_t(str[0])) * prime);
	}
#endif
}

// Public RTTI API
namespace RTTI
{
	using RTTI_Private::typeid_t;

	// Interface of TypeInfo
	struct TypeInfo
	{
		KCL_FORCEINLINE const char* GetName() const
		{
			return myName;
		}
		KCL_FORCEINLINE const char* GetTypeData() const
		{
			return (char*)(this + 1);
		}
		KCL_FORCEINLINE typeid_t GetTypeId() const
		{
			return *(typeid_t*)(GetTypeData() + sizeof(typeid_t));
		}
		inline intptr_t CastTo(intptr_t aPtr, typeid_t aTypeId) const
		{
			const char* data = GetTypeData();
			size_t byteIndex = 0;
			ptrdiff_t offset = 0;

			while (true)
			{
				typeid_t size = *reinterpret_cast<const typeid_t*>(data + byteIndex);
				byteIndex += sizeof(typeid_t);

				for (typeid_t i = 0; i < size; i++, byteIndex += sizeof(typeid_t))
				{
					if (*reinterpret_cast<const typeid_t*>(data + byteIndex) == aTypeId)
					{
						return aPtr + offset;
					}
				}

				offset = *reinterpret_cast<const ptrdiff_t*>(data + byteIndex);
				if (offset == 0)
				{
					return 0;
				}

				byteIndex += sizeof(ptrdiff_t);
			}
		}

		KCL_FORCEINLINE bool operator==(const TypeInfo& anOther) const { return GetTypeId() == anOther.GetTypeId(); }
		KCL_FORCEINLINE bool operator!=(const TypeInfo& anOther) const { return GetTypeId() != anOther.GetTypeId(); }

		const char* myName;
	};

	// Public interface to access type information
	// Always access through this or risk wrong behavior
	template<typename T>
	KCL_FORCEINLINE const TypeInfo* GetTypeInfo()
	{
		typedef typename std::decay<typename std::remove_cv<T>::type>::type Type;
		return RTTI_Private::GetTypeInfo<Type>::Get();
	}

	template<typename T>
	KCL_FORCEINLINE typeid_t GetTypeId()
	{
		return GetTypeInfo<T>()->GetTypeId();
	}

	template<typename Derived, typename Base>
	KCL_FORCEINLINE Derived DynamicCast(Base* aBasePtr)
	{
		static_assert(std::is_pointer<Derived>::value, "Return type must be a pointer");
		using DerivedObjectType = std::remove_pointer_t<Derived>;

		if constexpr (std::is_base_of<DerivedObjectType, Base>::value)
		{
			return static_cast<Derived>(aBasePtr);
		}
		else if (aBasePtr)
		{
			return reinterpret_cast<Derived>(aBasePtr->DynamicCast(GetTypeId<DerivedObjectType>()));
		}
		else
		{
			return nullptr;
		}
	}
}

namespace RTTI_Private
{
	template<typename Derived, typename Base>
	static ptrdiff_t ComputePointerOffset()
	{
		Derived* derivedPtr = (Derived*)1;
		Base* basePtr = static_cast<Base*>(derivedPtr);
		return (intptr_t)basePtr - (intptr_t)derivedPtr;
	}

#pragma pack(push, 1)

	// Specialization will contain the magic data.
	template<typename T>
	struct TypeData
	{
	};

	// Recursively populates the typeData
	// Layout of typeData:
	// [ typeid_t size, typeid_t firstTypeId ... typeid_t lastTypeId, ptrdiff_t offset/endMarker if = 0,
	// typeid_t size, typeid_t firstTypeId ... typeid_t lastTypeId, ptrdiff_t offset/endMarker if = 0... ]
	// Each block represents inherited types from a base, the first block doesn't need offset as it is implicitly 0
	// Therefore we can use the offset as an end marker, all other bases will have a positive offset
	template<typename... BaseTypes>
	struct BaseTypeData
	{
	};

	template <typename Type>
	struct BaseTypeData<std::enable_shared_from_this<Type>>
	{
		template <typename Derived>
		void FillBaseTypeData(std::ptrdiff_t, typeid_t&) {}
	};

	template<typename FirstBase, typename SecondBase, typename... Next>
	struct BaseTypeData<FirstBase, SecondBase, Next...>
	{
		template<typename Derived>
		void FillBaseTypeData(ptrdiff_t aOffset, typeid_t& outHeadSize)
		{
			myFirst.template FillBaseTypeData<Derived>(ComputePointerOffset<Derived, FirstBase>(), outHeadSize);

			myOffset = ComputePointerOffset<Derived, SecondBase>();
			myNext.template FillBaseTypeData<Derived>(myOffset, mySize);
		}

		BaseTypeData<FirstBase> myFirst;
		ptrdiff_t myOffset;
		typeid_t mySize;
		BaseTypeData<SecondBase, Next...> myNext;
	};

	template<typename Base>
	struct BaseTypeData<Base>
	{
		template<typename Derived>
		void FillBaseTypeData(ptrdiff_t aOffset, typeid_t& outHeadSize)
		{
			const TypeData<Base>* baseTypeId = (TypeData<Base>*)(GetTypeInfo<Base>::Get()->GetTypeData());

			// return size of head list
			outHeadSize = baseTypeId->mySize;

			const char* data = baseTypeId->GetData();
			size_t byteSize = baseTypeId->mySize * sizeof(typeid_t);

			// copy type list
			memcpy(myData, data, byteSize);

			size_t byteIndex = byteSize;
			ptrdiff_t offset = *reinterpret_cast<const ptrdiff_t*>(data + byteIndex);
			while (offset != 0)
			{
				// fill next offset and add pointer offset
				*reinterpret_cast<ptrdiff_t*>(myData + byteIndex) = offset + aOffset;
				byteIndex += sizeof(ptrdiff_t);

				// fill next size
				const typeid_t size = *reinterpret_cast<const typeid_t*>(data + byteIndex);
				*reinterpret_cast<typeid_t*>(myData + byteIndex) = size;
				byteSize = size * sizeof(typeid_t);
				byteIndex += sizeof(typeid_t);

				// copy types
				memcpy(myData + byteIndex, data + byteIndex, byteSize);
				byteIndex += byteSize;

				offset = *reinterpret_cast<const ptrdiff_t*>(data + byteIndex);
			}
		}

		// We only need the previous type data array, but not its size or end marker
		char myData[sizeof(TypeData<Base>) - sizeof(ptrdiff_t) - sizeof(typeid_t)];
	};

	// Actual implementation of TypeData<Type>
	template<typename Type, typename... BaseTypes>
	struct TypeDataImpl
	{
		TypeDataImpl()
		{
			myBaseTypeData.template FillBaseTypeData<Type>(0 /* No offset with first base */, mySize);
			mySize++; // Size is the base's size + 1 to account for current type id
		}

		const char* GetData() const
		{
			return (char*)&myTypeId;
		}

		typeid_t mySize;
		typeid_t myTypeId {};
		BaseTypeData<BaseTypes...> myBaseTypeData;
		ptrdiff_t myEndMarker {};
	};

	template<typename Type>
	struct TypeDataImpl<Type>
	{
		const char* GetData() const
		{
			return (char*)&myTypeId;
		}

		typeid_t mySize {1};
		typeid_t myTypeId {};
		ptrdiff_t myEndMarker {};
	};

	template<typename T>
	struct TypeInfoImpl
	{
		const RTTI::TypeInfo myInfo;
		const TypeData<T> myData;
	};

#pragma pack(pop)
}

// Common declaration
#define RTTI_TYPEINFO(TYPE)																	\
	template<>																				\
	struct GetTypeInfo<TYPE>																\
	{																						\
		static const RTTI::TypeInfo* Get()													\
		{																					\
			static TypeInfoImpl<TYPE> ourInstance {KCL_TOSTRING(TYPE), TypeData<TYPE>()};	\
			return &ourInstance.myInfo;														\
		}																					\
	};

// Use for all types, must include all directly inherited types in the macro
// Note: Ideally we could remove the Register macro by making it possible to register it all from withing the class,
// but this approach allows registering non-class types as well.
#define RTTI_REGISTER(...)																		\
	namespace RTTI_Private																		\
	{																							\
		template<>																				\
		struct TypeData<KCL_FIRST_ARG(__VA_ARGS__)> : public TypeDataImpl<__VA_ARGS__>			\
		{																						\
			TypeData()																			\
			{																					\
				constexpr auto id = HashString(KCL_TOSTRING(KCL_FIRST_ARG(__VA_ARGS__)));		\
				myTypeId = id;																	\
			}																					\
		};																						\
		RTTI_TYPEINFO(KCL_FIRST_ARG(__VA_ARGS__))												\
	}

// Use in the body of all polymorphic types
#define RTTI_IMPL()																						\
	virtual intptr_t DynamicCast(RTTI::typeid_t aOtherTypeId) const										\
	{																									\
		typedef std::remove_pointer<decltype(this)>::type ObjectType;									\
		return RTTI::template GetTypeInfo<ObjectType>()->CastTo((intptr_t)this, aOtherTypeId);			\
	}																									\
	virtual const RTTI::TypeInfo* GetTypeInfo() const													\
	{																									\
		typedef std::remove_pointer<decltype(this)>::type ObjectType;									\
		return RTTI::template GetTypeInfo<ObjectType>();												\
	}																									\
	virtual const char* GetTypeName() const																\
	{																									\
		return GetTypeInfo()->GetName();																\
	}																									\
	virtual RTTI::typeid_t GetTypeId() const															\
	{																									\
		typedef std::remove_pointer<decltype(this)>::type ObjectType;									\
		return RTTI::template GetTypeId<ObjectType>();													\
	}
