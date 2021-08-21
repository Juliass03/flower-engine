/** 
 * NOTE: 引擎的Auto Console Variable System
 *       大部分来源于UE4的IConsoleVariable System
 *       多线程下读写时使用了share_lock可能会稍微拖慢速度
**/
#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include <shared_mutex>
#include "noncopyable.h"
#include <algorithm> 

namespace engine{

constexpr auto CVAR_ARRAY_SIZE = 2000;

// CVar的可读写性标记
enum CVarFlags
{
	None = 0x00,

	ReadOnly     = 0x01 << 1, // 只读的CVar
	ReadAndWrite = 0x01 << 2, // 可读并且可写的CVar

	/**
	 * NOTE: CVar可能有两次初始化的机会
	 *       第一次是全局构造函数调用时
	 *       若项目中存了ini config文件，则可能读取ini文件中的键值发生第二次赋值
	 *       InitOnce标记保证了该CVar只会在构造函数时调用一次赋值
	**/
	InitOnce     = 0x01 << 3, 

	Max,
};

enum class CVarType : uint8_t
{
	None = 0x00,

	Int32,
	Double,
	Float,
	String,

	Max,
};

struct CVarParameter
{
	// 存储在CVarArray中的位置
	int32_t arrayIndex;
	CVarType type;

	/** 
	 * NOTE: 这里不直接存CVarFlags而是改为uint32_t
	 *       因为我们需要按位与来设置与判断标志信息
	**/
	uint32_t flag;

	const char* category;
	const char* name;
	const char* description;
};

template<typename Worker>
struct CVarStorage
{
	Worker initVal;    // 存储初始值以便可以回滚配置到默认
	Worker currentVal; // 当前的配置值
	CVarParameter* parameter = nullptr;
};

/**
 * NOTE: CVar数据存储的数组地址
 *       所有全局初始化的CVar都会填充到这里来
**/
template<typename Worker>
struct CVarArray : private NonCopyable
{
	CVarStorage<Worker>* cvars;
	int32_t lastCVar = 0;
	int32_t capacity;

	CVarArray(size_t size)
	{
		cvars = new CVarStorage<Worker>[size]();
		capacity = (uint32_t)size;
	}

	~CVarArray()
	{
		delete[] cvars;
	}

	inline CVarStorage<Worker>* getCurrentStorage(int32_t index)
	{
		return &cvars[index];
	}

	inline Worker* getCurrentPtr(int32_t index)
	{
		return &cvars[index].currentVal;
	}

	inline Worker getCurrent(int32_t index)
	{
		return cvars[index].currentVal;
	};

	inline void setCurrent(const Worker& val,int32_t index)
	{
		if(cvars[index].parameter->flag & InitOnce)
		{
			return;
		}
		cvars[index].currentVal = val;
	}

	inline int add(const Worker& value,CVarParameter* param)
	{
		int index = lastCVar;

		cvars[index].currentVal = value;
		cvars[index].initVal = value;
		cvars[index].parameter = param;
		param->arrayIndex = index;
		lastCVar++;

		return index;
	}

	inline int add(const Worker& initialValue,const Worker& currentValue,CVarParameter* param)
	{
		int index = lastCVar;

		cvars[index].currentVal = currentValue;
		cvars[index].initVal = initialValue;
		cvars[index].parameter = param;
		param->arrayIndex = index;
		lastCVar++;

		return index;
	}
};

/**
 * NOTE: CVarSystem单例类
 *       线程安全的Console Variable System
**/
class CVarSystem : private NonCopyable
{
public:
	static CVarSystem* get()
	{
		static CVarSystem cVarSystem{ };
		return &cVarSystem;
	}

	CVarArray<int32_t>& getInt32Array() { return m_int32CVars; }
	CVarArray<float>& getFloatArray() { return m_floatCVars; }
	CVarArray<double>& getDoubleArray() { return m_doubleCVars; }
	CVarArray<std::string>& getStringArray() { return m_stringCVars; }
private:
	inline size_t hash(std::string str)
	{
		std::transform(str.begin(), str.end(), str.begin(), ::tolower);
		size_t result = 0;
		for (auto it = str.cbegin(); it != str.cend(); ++it) 
		{  
			result = (result * 131) + *it;                                        
		}

		return result;
	}

	template <typename Worker>
	inline std::string toString(Worker v)
	{
		return std::to_string(v);
	}

	template <>
	inline std::string toString(std::string c)
	{
		std::string ret = "\""; ret += c; ret += "\"";
		return ret;
	}

private:
	constexpr static int MAX_INT32_CVARS  { CVAR_ARRAY_SIZE };
	constexpr static int MAX_FLOAT_CVARS  { CVAR_ARRAY_SIZE };
	constexpr static int MAX_DOUBLE_CVARS { CVAR_ARRAY_SIZE };
	constexpr static int MAX_STRING_CVARS { CVAR_ARRAY_SIZE };

	CVarArray<int32_t>     m_int32CVars { MAX_INT32_CVARS  };
	CVarArray<float>       m_floatCVars { MAX_FLOAT_CVARS  };
	CVarArray<double>      m_doubleCVars{ MAX_DOUBLE_CVARS };
	CVarArray<std::string> m_stringCVars{ MAX_STRING_CVARS };

	// 读写的lock
	std::shared_mutex m_lockMutex;
	std::unordered_map<size_t,CVarParameter> m_cacheCVars;

	inline CVarParameter* initCVar(const char* name,const char* description)
	{
		size_t hashId = hash(name);
		m_cacheCVars[hashId] = CVarParameter{ };
		auto& newParm = m_cacheCVars[hashId];
		newParm.name = name;
		newParm.description = description;
		return &newParm;
	}

	template<typename Worker>
	CVarArray<Worker>* getCVarArray();

	template<>
	CVarArray<int32_t>* getCVarArray()
	{
		return &m_int32CVars;
	}

	template<>
	CVarArray<float>* getCVarArray()
	{
		return &m_floatCVars;
	}

	template<>
	CVarArray<double>* getCVarArray()
	{
		return &m_doubleCVars;
	}

	template<>
	CVarArray<std::string>* getCVarArray()
	{
		return &m_stringCVars;
	}

	template<typename Worker>
	inline Worker* getCVarCurrent(const char* name)
	{
		CVarParameter* par = getCVar(name);
		if(!par)
		{
			return nullptr;
		}
		else
		{
			return getCVarArray<Worker>()->getCurrentPtr(par->arrayIndex);
		}
	}

	template<typename Worker>
	inline void setCVarCurrent(const char* name,const Worker& value)
	{
		CVarParameter* cvar = getCVar(name);

		if(cvar)
		{
			getCVarArray<Worker>()->setCurrent(value,cvar->arrayIndex);
		}
	}

public:
	CVarParameter* getCVar(const char* name)
	{
		// 使用lock防止条件竞争
		std::shared_lock<std::shared_mutex> lock(m_lockMutex);
		size_t hashKey = hash(name); 
		auto it = m_cacheCVars.find(hashKey);
		if(it != m_cacheCVars.end())
		{
			return &(*it).second;
		}

		return nullptr;
	}

	inline float* getFloatCVar(const char* name)
	{
		return getCVarCurrent<float>(name);
	}

	inline double* getDoubleCVar(const char* name)
	{
		return getCVarCurrent<double>(name);
	}

	inline int32_t* getInt32CVar(const char* name)
	{
		return getCVarCurrent<int32_t>(name);
	}

	inline std::string* getStringCVar(const char* name)
	{
		return getCVarCurrent<std::string>(name);
	}

	inline void setFloatCVar(const char* name,float value)
	{
		setCVarCurrent<float>(name,value);
	}

	inline void setDoubleCVar(const char* name,double value)
	{
		setCVarCurrent<double>(name,value);
	}

	inline void setInt32CVar(const char* name,int32_t value)
	{
		setCVarCurrent<int32_t>(name,value);
	}

	inline void setStringCVar(const char* name,const char* value)
	{
		setCVarCurrent<std::string>(name,value);
	}

private:
	template<typename BaseType>
	inline CVarParameter* addCVarTypeParam(
		CVarType type,
		const char* name,
		const char* description,
		const char* category,
		BaseType defaultValue,
		BaseType currentValue)
	{
		// 使用lock防止条件竞争
		std::unique_lock<std::shared_mutex> lock(m_lockMutex);
		CVarParameter* param = initCVar(name,description);
		if(!param) return nullptr; 

		param->type = type; 
		param->category = category; 
		getCVarArray<BaseType>()->add(defaultValue,currentValue,param);
		return param; 
	}

	inline CVarParameter* addFloatCVar(
		const char* name,
		const char* description,
		const char* category,
		float defaultValue,
		float currentValue)
	{
		return addCVarTypeParam<float>(
			CVarType::Float,
			name,
			description,
			category,
			defaultValue,
			currentValue);
	}

	inline CVarParameter* addDoubleCVar(
		const char* name,
		const char* description,
		const char* category,
		double defaultValue,
		double currentValue)
	{
		return addCVarTypeParam<double>(
			CVarType::Double,
			name,
			description,
			category,
			defaultValue,
			currentValue);
	}

	inline CVarParameter* addInt32CVar(
		const char* name,
		const char* description,
		const char* category,
		int32_t defaultValue,
		int32_t currentValue)
	{
		return addCVarTypeParam<int32_t>(
			CVarType::Int32,
			name,
			description,
			category,
			defaultValue,
			currentValue);
	}

	inline CVarParameter* addStringCVar(
		const char* name,
		const char* description,
		const char* category,
		const std::string& defaultValue,
		const std::string& currentValue)
	{
		return addCVarTypeParam<std::string>(
			CVarType::String,
			name,
			description,
			category,
			defaultValue,
			currentValue);
	}

	friend struct AutoCVarFloat;
	friend struct AutoCVarDouble;
	friend struct AutoCVarInt32;
	friend struct AutoCVarString;

	template<typename Ty> 
	friend Ty   getCVarCurrentByIndex(int32_t);
	template<typename Ty> 
	friend Ty*  ptrGetCVarCurrentByIndex(int32_t);
	template<typename Ty> 
	friend void setCVarCurrentByIndex(int32_t,const Ty&);
};

template<typename Worker>
struct AutoCVar
{
protected:
	int index;
	using CVarType = Worker;
};

struct AutoCVarFloat : AutoCVar<float>
{
	AutoCVarFloat(
		const char* name,
		const char* description,
		const char* category,
		float defaultValue,
		uint32_t flags = CVarFlags::None)
	{
		CVarParameter* cvar = CVarSystem::get()->addFloatCVar(
			name,
			description,
			category,
			defaultValue,
			defaultValue
		);

		cvar->flag = flags;
		index = cvar->arrayIndex;
	}

	inline float  get();
	inline float* getPtr();
	inline void   set(float val);
};

struct AutoCVarDouble: AutoCVar<double>
{
	AutoCVarDouble(
		const char* name,
		const char* description,
		const char* category,
		double defaultValue,
		uint32_t flags = CVarFlags::None)
	{
		CVarParameter* cvar = CVarSystem::get()->addDoubleCVar(
			name,
			description,
			category,
			defaultValue,
			defaultValue
		);

		cvar->flag = flags;
		index = cvar->arrayIndex;
	}

	inline double  get();
	inline double* getPtr();
	inline void    set(double val);
};

struct AutoCVarInt32 : AutoCVar<int32_t>
{
	AutoCVarInt32(
		const char* name,
		const char* description,
		const char* category,
		int32_t defaultValue,
		uint32_t flags = CVarFlags::None)
	{
		CVarParameter* cvar = CVarSystem::get()->addInt32CVar(
			name,
			description,
			category,
			defaultValue,
			defaultValue
		);

		cvar->flag = flags;
		index = cvar->arrayIndex;
	}

	inline int32_t  get();
	inline int32_t* getPtr();
	inline void     set(int32_t val);
};

struct AutoCVarString : AutoCVar<std::string>
{
	AutoCVarString(
		const char* name,
		const char* description,
		const char* category,
		const std::string& defaultValue,
		uint32_t flags = CVarFlags::None)
	{
		CVarParameter* cvar = CVarSystem::get()->addStringCVar(
			name,
			description,
			category,
			defaultValue,
			defaultValue
		);

		cvar->flag = flags;
		index = cvar->arrayIndex;
	}

	inline std::string get();
	inline std::string* getPtr();
	inline void        set(const std::string& val);
};

template<typename Worker>
inline Worker getCVarCurrentByIndex(int32_t index)
{
	return CVarSystem::get()->getCVarArray<Worker>()->getCurrent(index);
}

template<typename Worker>
inline Worker* ptrGetCVarCurrentByIndex(int32_t index)
{
	return CVarSystem::get()->getCVarArray<Worker>()->getCurrentPtr(index);
}

template<typename Worker>
inline void setCVarCurrentByIndex(int32_t index,const Worker& data)
{
	CVarSystem::get()->getCVarArray<Worker>()->setCurrent(data,index);
}

inline float AutoCVarFloat::get()
{
	return getCVarCurrentByIndex<CVarType>(index);
}

inline double AutoCVarDouble::get()
{
	return getCVarCurrentByIndex<CVarType>(index);
}

inline int32_t AutoCVarInt32::get()
{
	return getCVarCurrentByIndex<CVarType>(index);
}

inline std::string AutoCVarString::get()
{
	return getCVarCurrentByIndex<CVarType>(index);
}

inline void AutoCVarFloat::set(float f)
{
	setCVarCurrentByIndex<CVarType>(index,f);
}

inline void AutoCVarDouble::set(double f)
{
	setCVarCurrentByIndex<CVarType>(index,f);
}

inline void AutoCVarInt32::set(int32_t f)
{
	setCVarCurrentByIndex<CVarType>(index,f);
}

inline void AutoCVarString::set(const std::string& f)
{
	setCVarCurrentByIndex<CVarType>(index,f);
}

inline float* AutoCVarFloat::getPtr()
{
	return ptrGetCVarCurrentByIndex<CVarType>(index);
}

inline double* AutoCVarDouble::getPtr()
{
	return ptrGetCVarCurrentByIndex<CVarType>(index);
}

inline int32_t* AutoCVarInt32::getPtr()
{
	return ptrGetCVarCurrentByIndex<CVarType>(index);
}

inline std::string* AutoCVarString::getPtr()
{
	return ptrGetCVarCurrentByIndex<CVarType>(index);
}

}