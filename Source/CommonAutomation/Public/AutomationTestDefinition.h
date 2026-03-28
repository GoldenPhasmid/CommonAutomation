#pragma once

#if WITH_AUTOMATION_WORKER
#define BEGIN_DEFINE_CUSTOM_SPEC_PRIVATE( TClass, TBaseClass, PrettyName, TFlags, FileName, LineNumber ) \
	class TClass : public TBaseClass \
	{ \
	public: \
		TClass( const FString& InName ) \
		: TBaseClass( InName, false ) { \
		} \
		virtual EAutomationTestFlags GetTestFlags() const override { return TFlags; } \
		using FAutomationSpecBase::GetTestSourceFileName; \
		virtual FString GetTestSourceFileName() const override { return FileName; } \
		using FAutomationSpecBase::GetTestSourceFileLine; \
		virtual int32 GetTestSourceFileLine() const override { return LineNumber; } \
	protected: \
		virtual FString GetBeautifiedTestName() const override { return PrettyName; } \
		virtual void Define() override;

#define BEGIN_CUSTOM_SIMPLE_AUTOMATION_TEST_PRIVATE(TClass, TBaseClass, PrettyName, TFlags, FileName, LineNumber) \
	class TClass : public TBaseClass \
	{ \
	public: \
		TClass( const FString& InName ) \
		:TBaseClass( InName, false ) {\
		} \
		virtual EAutomationTestFlags GetTestFlags() const override { return TFlags; } \
		virtual bool IsStressTest() const { return false; } \
		virtual uint32 GetRequiredDeviceNum() const override { return 1; } \
		virtual FString GetTestSourceFileName() const override { return FileName; } \
		virtual int32 GetTestSourceFileLine() const override { return LineNumber; } \
	protected: \
	virtual void GetTests(TArray<FString>& OutBeautifiedNames, TArray <FString>& OutTestCommands) const override \
	{ \
		OutBeautifiedNames.Add(PrettyName); \
		OutTestCommands.Add(FString()); \
	} \
	virtual bool RunTest(const FString& Parameters) override; \
	virtual FString GetBeautifiedTestName() const override { return PrettyName; }

#define BEGIN_CUSTOM_SIMPLE_AUTOMATION_TEST(TClass, TBaseClass, PrettyName, TFlags) \
	BEGIN_CUSTOM_SIMPLE_AUTOMATION_TEST_PRIVATE(TClass, TBaseClass, PrettyName, TFlags, __FILE__, __LINE__)

#define END_CUSTOM_SIMPLE_AUTOMATION_TEST(TClass) \
	}; \
	namespace \
	{ \
		TClass TClass##AutomationTestInstance( TEXT(#TClass) ); \
	}

#define BEGIN_SIMPLE_AUTOMATION_TEST(TClass, PrettyName, TFlags) \
	BEGIN_CUSTOM_SIMPLE_AUTOMATION_TEST_PRIVATE(TClass, FAutomationTestBase, PrettyName, TFlags, __FILE__, __LINE__)

#define END_SIMPLE_AUTOMATION_TEST(TClass) \
	}; \
	namespace \
	{ \
		TClass TClass##AutomationTestInstance( TEXT(#TClass) ); \
	}

#define BEGIN_DEFINE_CUSTOM_SPEC(TClass, TBaseClass, PrettyName, TFlags) \
	BEGIN_DEFINE_CUSTOM_SPEC_PRIVATE(TClass, TBaseClass, PrettyName, TFlags, __FILE__, __LINE__)

#define END_DEFINE_CUSTOM_SPEC(TClass) \
	}; \
	namespace \
	{ \
		TClass TClass##AutomationSpecInstance( TEXT(#TClass) ); \
	}

#endif