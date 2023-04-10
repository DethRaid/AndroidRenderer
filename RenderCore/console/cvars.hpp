// cvars.h : console variable system

#pragma once

#include "string_utils.hpp"

class CVarParameter;

enum class CVarFlags : uint32_t
{
	None = 0,
	Noedit = 1 << 1,
	EditReadOnly = 1 << 2,
	Advanced = 1 << 3,

	EditCheckbox = 1 << 8,
	EditFloatDrag = 1 << 9,
};
class CVarSystem
{

public:
	static CVarSystem* Get();

	//pimpl
	virtual CVarParameter* GetCVar(StringUtils::StringHash hash) = 0;

	virtual double* GetFloatCVar(StringUtils::StringHash hash) = 0;

	virtual int32_t* GetIntCVar(StringUtils::StringHash hash) = 0;

	virtual const char* GetStringCVar(StringUtils::StringHash hash) = 0;

	virtual void SetFloatCVar(StringUtils::StringHash hash, double value) = 0;

	virtual void SetIntCVar(StringUtils::StringHash hash, int32_t value) = 0;

	virtual void SetStringCVar(StringUtils::StringHash hash, const char* value) = 0;

	
	virtual CVarParameter* CreateFloatCVar(const char* name, const char* description, double defaultValue, double currentValue) = 0;
	
	virtual CVarParameter* CreateIntCVar(const char* name, const char* description, int32_t defaultValue, int32_t currentValue) = 0;
	
	virtual CVarParameter* CreateStringCVar(const char* name, const char* description, const char* defaultValue, const char* currentValue) = 0;
	
	virtual void DrawImguiEditor() = 0;
};

template<typename T>
struct AutoCVar
{
protected:
	int index;
	using CVarType = T;
};

struct AutoCVar_Float : AutoCVar<double>
{
	AutoCVar_Float(const char* name, const char* description, double defaultValue, CVarFlags flags = CVarFlags::None);

	double Get();
	double* GetPtr();
	float GetFloat();
	float* GetFloatPtr();
	void Set(double val);
};

struct AutoCVar_Int : AutoCVar<int32_t>
{
	AutoCVar_Int(const char* name, const char* description, int32_t defaultValue, CVarFlags flags = CVarFlags::None);
	int32_t Get();
	int32_t* GetPtr();
	void Set(int32_t val);

	void Toggle();
};

struct AutoCVar_String : AutoCVar<std::string>
{
	AutoCVar_String(const char* name, const char* description, const char* defaultValue, CVarFlags flags = CVarFlags::None);

	const char* Get();
	void Set(std::string&& val);
};

template <typename EnumType>
struct AutoCVar_Enum : AutoCVar_Int {
	AutoCVar_Enum(const char* name, const char* description, EnumType defaultValue, CVarFlags flags = CVarFlags::None);
	EnumType Get();
	EnumType* GetPtr();
	void Set(EnumType val);
};

template <typename EnumType>
AutoCVar_Enum<EnumType>::AutoCVar_Enum(
	const char* name, const char* description, EnumType defaultValue, CVarFlags flags
) : AutoCVar_Int{ name, description, static_cast<int32_t>(defaultValue), flags } {}

template <typename EnumType>
EnumType AutoCVar_Enum<EnumType>::Get() {
	return static_cast<EnumType>(AutoCVar_Int::Get());
}

template <typename EnumType>
EnumType* AutoCVar_Enum<EnumType>::GetPtr() {
	return static_cast<EnumType*>(AutoCVar_Int::GetPtr());
}

template <typename EnumType>
void AutoCVar_Enum<EnumType>::Set(EnumType val) {
	AutoCVar_Int::Set(static_cast<int32_t>(val));
}
