#include "ArrayHelper.h"
#include "V8GlobalHelpers.h"
#include "JEnv.h"
#include "JniLocalRef.h"
#include "NativeScriptException.h"
#include <assert.h>
#include <sstream>

using namespace v8;
using namespace std;
using namespace tns;

ArrayHelper::ArrayHelper()
{
}

void ArrayHelper::Init(ObjectManager *objectManager, const Local<Context>& context)
{
	JEnv env;

	PLATFORM_CLASS = env.FindClass("com/tns/Platform");
	assert(PLATFORM_CLASS != nullptr);

	CREATE_ARRAY_HELPER = env.GetStaticMethodID(PLATFORM_CLASS, "createArrayHelper", "(Ljava/lang/String;I)Ljava/lang/Object;");
	assert(CREATE_ARRAY_HELPER != nullptr);

	s_objectManager = objectManager;

	auto global = context->Global();
	auto arr = global->Get(context, ConvertToV8String("Array"));

	if (!arr.IsEmpty())
	{
		Local<Value> arrVal;
		auto success = arr.ToLocal(&arrVal);
		if (success)
		{
			auto isolate = context->GetIsolate();
			auto arrayObj = arrVal.As<Object>();
			arrayObj->Set(context, ConvertToV8String("create"), FunctionTemplate::New(isolate, CreateJavaArrayCallback)->GetFunction());
		}
	}
}

void ArrayHelper::CreateJavaArrayCallback(const FunctionCallbackInfo<Value>& info)
{
	try
	{
		CreateJavaArray(info);
	}
	catch (NativeScriptException& e)
	{
		e.ReThrowToV8();
	}
	catch (std::exception e) {
		stringstream ss;
		ss << "Error: c++ exception: " << e.what() << endl;
		NativeScriptException nsEx(ss.str());
		nsEx.ReThrowToV8();
	}
	catch (...) {
		NativeScriptException nsEx(std::string("Error: c++ exception!"));
		nsEx.ReThrowToV8();
	}
}

void ArrayHelper::CreateJavaArray(const v8::FunctionCallbackInfo<v8::Value>& info)
{
	auto isolate = info.GetIsolate();
	auto context = isolate->GetCurrentContext();

	if (info.Length() != 2)
	{
		Throw(isolate, "Expect two parameters.");
		return;
	}

	auto type = info[0];
	auto length = info[1];

	JniLocalRef array;

	if (type->IsString())
	{
		if (!length->IsInt32())
		{
			Throw(isolate, "Expect integer value as a second argument.");
			return;
		}

		jint len = length->Int32Value(context).FromJust();
		if (len < 0)
		{
			Throw(isolate, "Expect non-negative integer value as a second argument.");
			return;
		}

		auto typeName = ConvertToString(type.As<String>());
		array = JniLocalRef(CreateArrayByClassName(typeName, len));
	}
	else if (type->IsFunction())
	{
		if (!length->IsInt32())
		{
			Throw(isolate, "Expect integer value as a second argument.");
			return;
		}

		jint len = length->Int32Value(context).FromJust();
		if (len < 0)
		{
			Throw(isolate, "Expect non-negative integer value as a second argument.");
			return;
		}

		auto func = type.As<Function>();

		auto clazz = func->Get(ConvertToV8String("class"));

		if (clazz.IsEmpty())
		{
			Throw(isolate, "Expect known class as a second argument.");
			return;
		}

		auto c = s_objectManager->GetJavaObjectByJsObjectStatic(clazz.As<Object>());

		JEnv env;
		array = env.NewObjectArray(len, static_cast<jclass>(c), nullptr);
	}
	else
	{
		Throw(isolate, "Expect primitive type name or class function as a first argument");
		return;
	}

	jint javaObjectID = s_objectManager->GetOrCreateObjectId(array);
	auto jsWrapper = s_objectManager->CreateJSWrapper(javaObjectID, "" /* ignored */, array);
	info.GetReturnValue().Set(jsWrapper);
}

void ArrayHelper::Throw(Isolate *isolate, const std::string& errorMessage)
{
	auto errMsg = ConvertToV8String(errorMessage.c_str());
	auto err = Exception::Error(errMsg);
	isolate->ThrowException(err);
}

jobject ArrayHelper::CreateArrayByClassName(const string& typeName, int length)
{
	JEnv env;
	jobject array;

	if (typeName == "char")
	{
		array = env.NewCharArray(length);
	}
	else if (typeName == "boolean")
	{
		array = env.NewBooleanArray(length);
	}
	else if (typeName == "byte")
	{
		array = env.NewByteArray(length);
	}
	else if (typeName == "short")
	{
		array = env.NewShortArray(length);
	}
	else if (typeName == "int")
	{
		array = env.NewIntArray(length);
	}
	else if (typeName == "long")
	{
		array = env.NewLongArray(length);
	}
	else if (typeName == "float")
	{
		array = env.NewFloatArray(length);
	}
	else if (typeName == "double")
	{
		array = env.NewDoubleArray(length);
	}
	else
	{
		JniLocalRef s(env.NewStringUTF(typeName.c_str()));
		array = env.CallStaticObjectMethod(PLATFORM_CLASS, CREATE_ARRAY_HELPER, (jstring)s, length);
	}

	return array;
}

ObjectManager* ArrayHelper::s_objectManager = nullptr;
jclass ArrayHelper::PLATFORM_CLASS = nullptr;
jmethodID ArrayHelper::CREATE_ARRAY_HELPER = nullptr;