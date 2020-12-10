/*
  ==============================================================================

    This file was auto-generated!

  ==============================================================================
*/

#include "MainComponent.h"

using namespace snex::Types;
using namespace snex::jit;
using namespace snex;



/** A class that wraps a SNEX compiled object and supplies a predefined
	function list that can be called from C++
*/
struct JitObjectWithPrototype
{
	/** Create an object with the given code and supply a scope. */
	JitObjectWithPrototype(snex::jit::GlobalScope& s_) :
		s(s_)
	{}



	template <int FunctionIndex, typename ReturnType, typename... Parameters>
	ReturnType call(Parameters... ps)
	{
		auto ptr = reinterpret_cast<void*>(data.get());
		return memberFunctions[FunctionIndex].callUnchecked<ReturnType>(ptr, ps...);
	}

	virtual int getNumChannels() const = 0;

	virtual void compiled(snex::jit::Compiler& c, Result r) = 0;

	virtual Array<Identifier> getFunctionList() const = 0;

	/** Register all functions that you want to call. */
	Result compile(const juce::String& code)
	{

		ok = false;
		snex::jit::Compiler c(s);
		SnexObjectDatabase::registerObjects(c, getNumChannels());
		obj = c.compileJitObject(code);

		if (!c.getCompileResult().wasOk())
		{
			auto r = c.getCompileResult();
			compiled(c, r);
			return r;
		}

		if (auto type = c.getComplexType(snex::NamespacedIdentifier("instance"), {}))
		{
			objectSize = type->getRequiredByteSize();

			data.calloc(objectSize);

			snex::jit::ComplexType::InitData d;
			d.dataPointer = data.get();
			d.initValues = type->makeDefaultInitialiserList();

			type->initialise(d);

			snex::jit::FunctionClass::Ptr fc = type->getFunctionClass();

			if (fc == nullptr)
			{
				auto r = Result::fail("Main object does not have member functions");
				compiled(c, r);
				return r;
			}

			for (auto id : getFunctionList())
			{
				auto fId = fc->getClassName().getChildId(id);
				auto fData = fc->getNonOverloadedFunction(fId);

				if (!fData.isResolved())
				{
					auto r = Result::fail("Can't find function " + id.toString());
					compiled(c, r);
					return r;
				}


				fData.object = data;
				memberFunctions.add(fData);
			}
		}
		else
		{
			auto r = Result::fail("Can't find instance type");
			compiled(c, r);
			return r;
		}

		ok = true;

		auto r = Result::ok();
		compiled(c, r);
		return r;
	}

	bool ok = false;

	snex::jit::GlobalScope& s;
	snex::jit::JitObject obj;
	HeapBlock<uint8> data;
	size_t objectSize = 0;

	Array<snex::jit::FunctionData> memberFunctions;
};



template <int NumChannels> struct ScriptNodeClassPrototype : public JitObjectWithPrototype
{
	

	ScriptNodeClassPrototype(snex::jit::GlobalScope& s) :
		JitObjectWithPrototype(s)
	{

	}

	int getNumChannels() const override { return NumChannels; }

	virtual void compiled(snex::jit::Compiler& c, Result r) override
	{

	}

	Array<Identifier> getFunctionList() const override
	{
		return { "reset", "process", "processFrame", "prepare", "handleEvent" };
	}

	void reset()
	{
		if (ok)
			call<ScriptnodeCallbacks::ResetFunction, void>();
	}

	void process(ProcessDataDyn& d)
	{
		if (ok)
		{
			auto f = d.toFix<NumChannels>();
			call<ScriptnodeCallbacks::ProcessFunction, void>(&f);
		}
	}

	void processSingle(float* data, int numChannels)
	{
		if (ok)
		{
			auto& d = span<float, NumChannels>::fromExternalData(data, numChannels);
			call<ScriptnodeCallbacks::ProcessSingleFunction, void>(&d);
		}
	}

	void prepare(PrepareSpecs ps)
	{
		ps.numChannels = NumChannels;

		if (ok)
			call<ScriptnodeCallbacks::PrepareFunction, void>(&ps);
	}

	void handleEvent(HiseEvent& e)
	{
		if (ok)
			call<ScriptnodeCallbacks::HandleEventFunction, void>(&e);
	}
};

juce::String getEmpty(const Identifier& namespaceId)
{
	juce::String s;
	juce::String nl = "\n";

	s << "struct " << namespaceId.toString() << nl;
	s << "{" << nl;
	s << "    void reset() {};" << nl;
	s << "    void prepare(PrepareSpecs ps) {};" << nl;
	s << "    void process(ProcessData& d){};" << nl;
	s << "    void handleEvent(HiseEvent& e){};" << nl;
	s << "};" << nl << nl;
	s << "using instance = " << namespaceId.toString() << ";" << nl;

	return s;
}





//==============================================================================
MainComponent::MainComponent() :
	data(new ui::WorkbenchData()),
	playground(new SnexPlayground(data, true))
{
	data->setContentFunctions([this]() {return v.toString(); }, [this](const String& s) {v.setValue(s); return true; });

	context.attachTo(*playground);
	
	addAndMakeVisible(playground);
    setSize (1024, 768);
}

MainComponent::~MainComponent()
{
	context.detach();


}

//==============================================================================
void MainComponent::paint (Graphics& g)
{
	g.fillAll(Colour(0xFF333336));
}

void MainComponent::resized()
{
    playground->setBounds(getLocalBounds());
}
