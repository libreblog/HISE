/*  ===========================================================================
*
*   This file is part of HISE.
*   Copyright 2016 Christoph Hart
*
*   HISE is free software: you can redistribute it and/or modify
*   it under the terms of the GNU General Public License as published by
*   the Free Software Foundation, either version 3 of the License, or
*   (at your option) any later version.
*
*   HISE is distributed in the hope that it will be useful,
*   but WITHOUT ANY WARRANTY; without even the implied warranty of
*   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*   GNU General Public License for more details.
*
*   You should have received a copy of the GNU General Public License
*   along with HISE.  If not, see <http://www.gnu.org/licenses/>.
*
*   Commercial licenses for using HISE in an closed source project are
*   available on request. Please visit the project's website to get more
*   information about commercial licensing:
*
*   http://www.hise.audio/
*
*   HISE is based on the JUCE library,
*   which also must be licenced for commercial applications:
*
*   http://www.juce.com
*
*   ===========================================================================
*/

#ifndef SCRIPTPROCESSOR_H_INCLUDED
#define SCRIPTPROCESSOR_H_INCLUDED
namespace hise { using namespace juce;



class ModulatorSynthGroup;



/** A class that has a content that can be populated with script components. 
*	@ingroup processor_interfaces
*
*	This is tightly coupled with the JavascriptProcessor class (so every JavascriptProcessor must also be derived from this class).
*/
class ProcessorWithScriptingContent: public SuspendableTimer::Manager
{
public:

	ProcessorWithScriptingContent(MainController* mc_) :
		restoredContentValues(ValueTree("Content")),
		mc(mc_),
		contentParameterHandler(*this)
	{}

	enum EditorStates
	{
		contentShown = 0,
		onInitShown,
		numEditorStates
	};

	virtual ~ProcessorWithScriptingContent();;

	void setAllowObjectConstruction(bool shouldBeAllowed)
	{
		allowObjectConstructors = shouldBeAllowed;
	}

	bool objectsCanBeCreated() const
	{
		return allowObjectConstructors;
	}

	virtual int getCallbackEditorStateOffset() const { return Processor::EditorState::numEditorStates; }

	void suspendStateChanged(bool shouldBeSuspended) override;

	ScriptingApi::Content *getScriptingContent() const
	{
		return content.get();
	}

	Identifier getContentParameterIdentifier(int parameterIndex) const
	{
		if (auto sc = content->getComponent(parameterIndex))
			return sc->name.toString();

		auto child = content->getContentProperties().getChild(parameterIndex);

		if (child.isValid())
			return Identifier(child.getProperty("id").toString());

		return Identifier();
	}

	void setControlValue(int index, float newValue);

	float getControlValue(int index) const;

	virtual void controlCallback(ScriptingApi::Content::ScriptComponent *component, var controllerValue);

	virtual int getControlCallbackIndex() const = 0;

	virtual int getNumScriptParameters() const;

	var getSavedValue(Identifier name)
	{
		return restoredContentValues.getChildWithName(name).getProperty("value", var::undefined());
	}

	void restoreContent(const ValueTree &restoredState);

	void saveContent(ValueTree &savedState) const;

	MainController* getMainController_() { return mc; }

	const MainController* getMainController_() const { return mc; }

protected:

	/** Call this from the base class to create the content. */
	void initContent()
	{
		content = new ScriptingApi::Content(this);
	}

	friend class JavascriptProcessor;

	JavascriptProcessor* thisAsJavascriptProcessor = nullptr;

	MainController* mc;

	bool allowObjectConstructors;

	ValueTree restoredContentValues;
	
	ReferenceCountedObjectPtr<ScriptingApi::Content> content;

	//WeakReference<ScriptingApi::Content> content;

	struct ContentParameterHandler: public ScriptParameterHandler
	{
		ContentParameterHandler(ProcessorWithScriptingContent& parent):
			p(parent)
		{}

		void setParameter(int index, float newValue) final override
		{
			p.setControlValue(index, newValue);
		}

		float getParameter(int index) const final override
		{
			return p.getControlValue(index);
		}

		int getNumParameters() const final override
		{
			return p.getNumScriptParameters();
		}

		Identifier getParameterId(int parameterIndex) const final override
		{
			return p.getContentParameterIdentifier(parameterIndex);
		}

		ProcessorWithScriptingContent& p;
	} contentParameterHandler;

private:

	void defaultControlCallbackIdle(ScriptingApi::Content::ScriptComponent *component, const var& controllerValue, Result& r);

	void customControlCallbackIdle(ScriptingApi::Content::ScriptComponent *component, const var& controllerValue, Result& r);

	JUCE_DECLARE_WEAK_REFERENCEABLE(ProcessorWithScriptingContent);
};


/** This class acts as base class for both ScriptProcessor and HardcodedScriptProcessor.
*
*	It contains all logic that the ScriptingApi objects need in order to work with both types.
*/
class ScriptBaseMidiProcessor: public MidiProcessor,
							   public ProcessorWithScriptingContent
{
public:

	enum Callback
	{
		onInit = 0,
		onNoteOn,
		onNoteOff,
		onController,
		onTimer,
		onControl,
		numCallbacks
	};

	ScriptBaseMidiProcessor(MainController *mc, const String &id): 
		MidiProcessor(mc, id), 
		ProcessorWithScriptingContent(mc),
		currentEvent(nullptr)
	{};

	virtual ~ScriptBaseMidiProcessor() { masterReference.clear(); }

	float getAttribute(int index) const override { return getControlValue(index); }
	void setInternalAttribute(int index, float newValue) override { setControlValue(index, newValue); }
	float getDefaultValue(int index) const override;

	ValueTree exportAsValueTree() const override { ValueTree v = MidiProcessor::exportAsValueTree(); saveContent(v); return v; }
	void restoreFromValueTree(const ValueTree &v) override { MidiProcessor::restoreFromValueTree(v); restoreContent(v); }

	const HiseEvent* getCurrentHiseEvent() const
	{
		return currentEvent;
	}


	int getControlCallbackIndex() const override { return onControl; };

	Identifier getIdentifierForParameterIndex(int parameterIndex) const override
	{
		return getContentParameterIdentifier(parameterIndex);
	}

protected:

	WeakReference<ScriptBaseMidiProcessor>::Master masterReference;
    friend class WeakReference<ScriptBaseMidiProcessor>;
	
	HiseEvent* currentEvent;

};



class ExternalScriptFile;

class FileChangeListener
{
public:

	virtual ~FileChangeListener();

	virtual void fileChanged() = 0;

	void addFileWatcher(const File &file);

	void setFileResult(const File &file, Result r);

	Result getWatchedResult(int index);

	void clearFileWatchers()
	{
		watchers.clear();
	}

	int getNumWatchedFiles() const noexcept
	{
		return watchers.size();
	}

	File getWatchedFile(int index) const;

	CodeDocument& getWatchedFileDocument(int index);

	void setCurrentPopup(DocumentWindow *window)
	{
		currentPopups.add(window);
	}

	void deleteAllPopups()
	{
		if (currentPopups.size() != 0)
		{
			for (int i = 0; i < currentPopups.size(); i++)
			{
				if (currentPopups[i].getComponent() != nullptr)
				{
					currentPopups[i]->closeButtonPressed();
				}

			}

			currentPopups.clear();
		}
	}

	void showPopupForFile(const File& f, int charNumberToDisplay = 0, int lineNumberToDisplay = -1);

	void showPopupForFile(int index, int charNumberToDisplay=0, int lineNumberToDisplay=-1);

	/** This includes every external script, compresses it and returns a base64 encoded string that can be shared without further dependencies. */
	static ValueTree collectAllScriptFiles(ModulatorSynthChain *synthChainToExport);

private:

	friend class ExternalScriptFile;
	friend class WeakReference < FileChangeListener > ;

	WeakReference<FileChangeListener>::Master masterReference;

	CodeDocument emptyDoc;

	ReferenceCountedArray<ExternalScriptFile> watchers;

	Array<Component::SafePointer<DocumentWindow>> currentPopups;

	static void addFileContentToValueTree(ValueTree externalScriptFiles, File scriptFile, ModulatorSynthChain* chainToExport);
};


class ComplexDataHolder: public SliderPackProcessor,
						 public LookupTableProcessor
{
public:

	virtual ~ComplexDataHolder() {};

	SliderPackData* getSliderPackData(int index) override
	{
		if (auto d = sliderPacks[index])
			return d->getSliderPackData();

		return nullptr;
	}

	const SliderPackData* getSliderPackData(int index) const override
	{
		if (auto d = sliderPacks[index])
			return d->getSliderPackData();

		return nullptr;
	}

	Table* getTable(int index) const override
	{
		if (auto d = tables[index])
			return d->getTable();

		return nullptr;
	}

	int getNumTables() const override { return tables.size(); };

	int getNumSliderPacks() const override { return sliderPacks.size(); }

	int getNumAudioFiles() const { return audioFiles.size(); };

	void saveComplexDataTypeAmounts(ValueTree& v) const
	{
		if (!sliderPacks.isEmpty())
			v.setProperty("NumSliderPacks", sliderPacks.size(), nullptr);

		if (!tables.isEmpty())
			v.setProperty("NumTables", tables.size(), nullptr);

		if (!audioFiles.isEmpty())
			v.setProperty("NumAudioFiles", audioFiles.size(), nullptr);
	}

	void restoreComplexDataTypes(const ValueTree& v)
	{
		auto pc = dynamic_cast<ProcessorWithScriptingContent*>(this);

		int numSliderPacks = v.getProperty("NumSliderPacks", 0);

		if (numSliderPacks > 0)
		{
			sliderPacks.ensureStorageAllocated(numSliderPacks);

			for (int i = 0; i < numSliderPacks; i++)
				sliderPacks.add(new ScriptingObjects::ScriptSliderPackData(pc));
		}

		int numTables = v.getProperty("NumTables", 0);

		if (numTables > 0)
		{
			tables.ensureStorageAllocated(numTables);

			for (int i = 0; i < numTables; i++)
				tables.add(new ScriptingObjects::ScriptTableData(pc));
		}

		int numAudioFiles = v.getProperty("NumAudioFiles", 0);

		if (numAudioFiles > 0)
		{
			audioFiles.ensureStorageAllocated(numAudioFiles);

			for (int i = 0; i < numAudioFiles; i++)
				audioFiles.add(new ScriptingObjects::ScriptAudioFile(pc));
		}
	}

	ScriptingObjects::ScriptSliderPackData* addOrReturnSliderPackObject(int index)
	{
		if (auto d = sliderPacks[index])
			return d;

		sliderPacks.set(index, new ScriptingObjects::ScriptSliderPackData(dynamic_cast<ProcessorWithScriptingContent*>(this)));
		return sliderPacks[index];
	}

	ScriptingObjects::ScriptAudioFile* addOrReturnAudioFile(int index)
	{
		if (auto d = audioFiles[index])
			return d;

		audioFiles.set(index, new ScriptingObjects::ScriptAudioFile(dynamic_cast<ProcessorWithScriptingContent*>(this)));
		return audioFiles[index];
	}

	ScriptingObjects::ScriptTableData* addOrReturnTableObject(int index)
	{
		if (auto d = tables[index])
			return d;

		tables.set(index, new ScriptingObjects::ScriptTableData(dynamic_cast<ProcessorWithScriptingContent*>(this)));
		return tables[index];
	}

protected:

	ReferenceCountedArray<ScriptingObjects::ScriptSliderPackData> sliderPacks;
	ReferenceCountedArray<ScriptingObjects::ScriptTableData> tables;
	ReferenceCountedArray<ScriptingObjects::ScriptAudioFile> audioFiles;

	JUCE_DECLARE_WEAK_REFERENCEABLE(ComplexDataHolder);
};





/** The base class for modules that can be scripted. 
*	@ingroup processor_interfaces
*	
*/
class JavascriptProcessor :	public FileChangeListener,
							public HiseJavascriptEngine::Breakpoint::Listener,
							public Dispatchable,
							public ComplexDataHolder,
							public ApiProviderBase::Holder,
							public scriptnode::DspNetwork::Holder
{
public:

	// ================================================================================================================


	

	/** A named document that contains a callback function. */
	class SnippetDocument : public CodeDocument
	{
	public:

		/** Create a snippet document. 
		*
		*	If you want to supply parameters, supply a whitespace separated list as last argument:
		*
		*	SnippetDocument doc("onControl", "component value"); // 2 parameters, 'component' and 'value'.
		*/
		SnippetDocument(const Identifier &callbackName_, const String &parameters=String());

        ~SnippetDocument()
        {
            SpinLock::ScopedLockType sl(pendingLock);
            notifier.cancelPendingUpdate();
            pendingNewContent = {};
        }
        
		/** Returns the name of the SnippetDocument. */
		const Identifier &getCallbackName() const { return callbackName; };

		/** Checks if the document contains code. */
		void checkIfScriptActive();

		/** Returns the function text. */
		String getSnippetAsFunction() const;

		/** Checks if the snippet contains any code to execute. This is a very fast operation. */
		bool isSnippetEmpty() const { return !isActive; };

		/** Returns the number of arguments specified in the constructor. */
		int getNumArgs() const { return numArgs; }

		void replaceContentAsync(String s)
		{
            
#if USE_FRONTEND
            // Not important when using compiled plugins because there will be no editor component
            // being resized...
            replaceAllContent(s);
#else
			// Makes sure that this won't be accessed during replacement...
			
			SpinLock::ScopedLockType sl(pendingLock);
			pendingNewContent.swapWith(s);
			notifier.notify();
#endif
		}

		

	private:

		/** This is necessary in order to avoid sending change messages to its Listeners
		*
		*	while compiling on another thread...
		*/
		struct Notifier: public AsyncUpdater
		{
			Notifier(SnippetDocument& parent_):
				parent(parent_)
			{

			}

			void notify()
			{
				triggerAsyncUpdate();
			}

		private:

			void handleAsyncUpdate() override
			{
				String text;
				
				{
					SpinLock::ScopedLockType sl(parent.pendingLock);
					parent.pendingNewContent.swapWith(text);
				}

                parent.setDisableUndo(true);
                
				parent.replaceAllContent(text);
                
                parent.setDisableUndo(false);
                
                
				parent.pendingNewContent = String();
			}

			SnippetDocument& parent;
		};

		SpinLock pendingLock;

        Notifier notifier;
		String pendingNewContent;

		Identifier callbackName;
		
		StringArray parameters;
		int numArgs;

		String emptyText;
		bool isActive;
	};
	
	// ================================================================================================================

	/** A Result object that contains the snippet. */
	struct SnippetResult
	{
		SnippetResult(Result r_, int c_) : r(r_), c(c_) {};

		/** the result */
		Result r;

		/** the callback */
		int c;
	};

	using ResultFunction = std::function<void(const SnippetResult& result)>;

	enum ScriptContextActions
	{
		SaveScriptFile = 9000,
		LoadScriptFile,
		SaveScriptClipboard,
		LoadScriptClipboard,

		ClearAllBreakpoints,


		CreateUiFactoryMethod,
		ReplaceConstructorWithReference,
		OpenExternalFile,
		OpenInPopup,
		MoveToExternalFile,
		InsertExternalFile,
		ExportAsCompressedScript,
		ImportCompressedScript
	};

	// ================================================================================================================

	JavascriptProcessor(MainController *mc);
	virtual ~JavascriptProcessor();

	struct EditorHelpers
	{
		static void applyChangesFromActiveEditor(JavascriptProcessor* p);

		static JavascriptCodeEditor* getActiveEditor(JavascriptProcessor* p);

		static JavascriptCodeEditor* getActiveEditor(Processor* p);

		static CodeDocument* gotoAndReturnDocumentWithDefinition(Processor* p, DebugableObjectBase* object);
	};

	ValueTree createApiTree() override
	{
#if USE_BACKEND
		return ApiHelpers::getApiTree();
#else
		return {};
#endif
	}

	void addPopupMenuItems(PopupMenu &m, Component* c, const MouseEvent &e) override;
	void performPopupMenuAction(int menuId, Component* c) override;

	SET_PROCESSOR_CONNECTOR_TYPE_ID("ScriptProcessor");

	void handleBreakpoints(const Identifier& codefile, Graphics& g, Component* c);

	void handleBreakpointClick(const Identifier& codeFile, CodeEditorComponent& ed, const MouseEvent& e);

#if USE_BACKEND
	bool handleKeyPress(const KeyPress& k, Component* c) override;
#endif

	void jumpToDefinition(const String& namespaceId, const String& token) override;

	void setActiveEditor(JavascriptCodeEditor* e, CodeDocument::Position pos) override;

	int getCodeFontSize() const override { return (int)dynamic_cast<const Processor*>(this)->getMainController()->getGlobalCodeFontSize(); }

	

	JavascriptCodeEditor* getActiveEditor() override;

	void breakpointWasHit(int index) override
	{
		for (int i = 0; i < breakpoints.size(); i++)
		{
			breakpoints.getReference(i).hit = (i == index);
		}

		for (int i = 0; i < breakpointListeners.size(); i++)
		{
			if (breakpointListeners[i].get() != nullptr)
			{
				breakpointListeners[i]->breakpointWasHit(index);
			}
		}

		if(index != -1)
			repaintUpdater.triggerAsyncUpdate();
	}

	

	virtual void fileChanged() override;

	void compileScript(const ResultFunction& f = ResultFunction());

	void setupApi();

	virtual void registerApiClasses() = 0;
	void registerCallbacks();

	virtual SnippetDocument *getSnippet(int c) = 0;
	virtual const SnippetDocument *getSnippet(int c) const = 0;
	virtual int getNumSnippets() const = 0;

	SnippetDocument *getSnippet(const Identifier& id);
	const SnippetDocument *getSnippet(const Identifier& id) const;

	void saveScript(ValueTree &v) const;
	void restoreScript(const ValueTree &v);

	void restoreInterfaceData(ValueTree propertyData);

	String collectScript(bool silent) const;

	String getBase64CompressedScript(bool silent=false) const;

	

	bool restoreBase64CompressedScript(const String &base64compressedScript);

	void setConnectedFile(const String& fileReference, bool compileScriptAfterLoad=true);

	bool isConnectedToExternalFile() const { return connectedFileReference.isNotEmpty(); }

	const String& getConnectedFileReference() const { return connectedFileReference; }

	void disconnectFromFile();

	void reloadFromFile();

	bool wasLastCompileOK() const { return lastCompileWasOK; }

	Result getLastErrorMessage() const { return lastResult; }

	ApiProviderBase* getProviderBase() override { return scriptEngine.get(); }

	HiseJavascriptEngine *getScriptEngine() { return scriptEngine; }

	void mergeCallbacksToScript(String &x, const String& sepString=String()) const;
	bool parseSnippetsFromString(const String &x, bool clearUndoHistory = false);

	void setCompileProgress(double progress);

	void compileScriptWithCycleReferenceCheckEnabled();

	void stuffAfterCompilation(const SnippetResult& r);

	void showPopupForCallback(const Identifier& callback, int charNumber, int lineNumber);

	void toggleBreakpoint(const Identifier& snippetId, int lineNumber, int charNumber)
	{
		HiseJavascriptEngine::Breakpoint bp(snippetId, "", lineNumber, charNumber, charNumber, breakpoints.size());

		int index = breakpoints.indexOf(bp);

		if (index != -1)
		{
			breakpoints.remove(index);
		}
		else
		{
			breakpoints.add(bp);
		}

		compileScript();
	}

	HiseJavascriptEngine::Breakpoint getBreakpointForLine(const Identifier &id, int lineIndex)
	{
		for (int i = 0; i < breakpoints.size(); i++)
		{
			if (breakpoints[i].snippetId == id && breakpoints[i].lineNumber == lineIndex)
				return breakpoints[i];
		}

		return HiseJavascriptEngine::Breakpoint();
	}

	void getBreakPointsForDisplayedRange(const Identifier& snippetId, Range<int> displayedLineNumbers, Array<int> &lineNumbers)
	{
		for (int i = 0; i < breakpoints.size(); i++)
		{
			if (breakpoints[i].snippetId != snippetId) continue;

			if (displayedLineNumbers.contains(breakpoints[i].lineNumber))
			{
				lineNumbers.add(breakpoints[i].lineNumber);
			}
		}
	}

	bool anyBreakpointsActive() const { return breakpoints.size() != 0; }

	void removeAllBreakpoints()
	{
		breakpoints.clear();

		compileScript();
	}

	void cleanupEngine();

	void setCallStackEnabled(bool shouldBeEnabled);

	void addBreakpointListener(HiseJavascriptEngine::Breakpoint::Listener* newListener)
	{
		breakpointListeners.addIfNotAlreadyThere(newListener);
	}

	void removeBreakpointListener(HiseJavascriptEngine::Breakpoint::Listener* listenerToRemove)
	{
		breakpointListeners.removeAllInstancesOf(listenerToRemove);
	}

	ScriptingApi::Content* getContent()
	{
		return dynamic_cast<ProcessorWithScriptingContent*>(this)->getScriptingContent();
	}
	
	const ScriptingApi::Content* getContent() const
	{
		return dynamic_cast<const ProcessorWithScriptingContent*>(this)->getScriptingContent();
	}

	

	void clearContentPropertiesDoc()
	{
		contentPropertyDocument = nullptr;
	}

	void createUICopyFromDesktop();

	void setDeviceTypeForInterface(int newDevice);

	ValueTree getContentPropertiesForDevice(int deviceIndex=-1);

	bool hasUIDataForDeviceType(int type=-1) const;

protected:

	void clearExternalWindows();

	friend class ProcessorWithScriptingContent;

	/** Overwrite this when you need to do something after the script was recompiled. */
	virtual void postCompileCallback() {};

	// ================================================================================================================

	class CompileThread : public ThreadWithProgressWindow
	{
	public:

		CompileThread(JavascriptProcessor *processor);

		void run();

		JavascriptProcessor::SnippetResult result;

	private:

		AlertWindowLookAndFeel alaf;
		JavascriptProcessor *sp;
	};

	// ================================================================================================================

	Result lastResult;

	virtual SnippetResult compileInternal();

	friend class CompileThread;

	String connectedFileReference;

	CompileThread *currentCompileThread;

	ScopedPointer<HiseJavascriptEngine> scriptEngine;

	MainController* mainController;

	bool lastCompileWasOK;
	bool useStoredContentData = false;

private:

	

	struct Helpers
	{
		static String resolveIncludeStatements(String& x, Array<File>& includedFiles, const JavascriptProcessor* p);
		static String stripUnusedNamespaces(const String &code, int& counter);
		static String uglify(const String& prettyCode);

	};

	

	UpdateDispatcher repaintDispatcher;

	

	Array<HiseJavascriptEngine::Breakpoint> breakpoints;

	Array<WeakReference<HiseJavascriptEngine::Breakpoint::Listener>> breakpointListeners;

	Array<Component::SafePointer<DocumentWindow>> callbackPopups;

	bool callStackEnabled = false;

	bool cycleReferenceCheckEnabled = false;

	

	ScopedPointer<CodeDocument> contentPropertyDocument;

	ValueTree allInterfaceData;

	JUCE_DECLARE_WEAK_REFERENCEABLE(JavascriptProcessor);

public:
	
};


class JavascriptThreadPool : public Thread,
							 public ControlledObject
{
public:

	JavascriptThreadPool(MainController* mc) :
		Thread("Javascript Thread"),
		ControlledObject(mc),
		lowPriorityQueue(8192),
		highPriorityQueue(2048),
		compilationQueue(128),
		deferredPanels(1024)
	{
		startThread(6);
	}

	~JavascriptThreadPool()
	{
		stopThread(1000);
	}

	void cancelAllJobs()
	{
		LockHelpers::SafeLock ss(getMainController(), LockHelpers::ScriptLock);

		stopThread(1000);
		compilationQueue.clear();
		lowPriorityQueue.clear();
		highPriorityQueue.clear();
		deferredPanels.clear();
	}
	
	class Task
	{
	public:

		enum Type
		{
			Compilation,
			HiPriorityCallbackExecution,
			LowPriorityCallbackExecution,
			DeferredPanelRepaintJob,
			Free,
			numTypes
		};

		using Function = std::function < Result(JavascriptProcessor*)>;

		Task() noexcept:
		  type(Free),
		  f(),
		  jp(nullptr)
		{};

		Task(Type t, JavascriptProcessor* jp_, const Function& functionToExecute) noexcept:
			type(t),
			f(functionToExecute),
			jp(jp_)
		{}

		JavascriptProcessor* getProcessor() const noexcept { return jp.get(); };
		
		Type getType() const noexcept { return type; }

		Result callWithResult();

		bool isValid() const noexcept { return (bool)f; }

		bool isHiPriority() const noexcept { return type == Compilation || type == HiPriorityCallbackExecution; }

	private:

		Type type;
		WeakReference<JavascriptProcessor> jp;
		Function f;
	};

	void addJob(Task::Type t, JavascriptProcessor* p, const Task::Function& f);

	void addDeferredPaintJob(ScriptingApi::Content::ScriptPanel* sp);

	void run() override;;

	const CriticalSection& getLock() const noexcept { return scriptLock; };

	bool isBusy() const noexcept { return busy; }

	Task::Type getCurrentTask() const noexcept { return currentType; }

	void killVoicesAndExtendTimeOut(JavascriptProcessor* jp, int milliseconds=1000);

private:

	using PendingCompilationList = Array<WeakReference<JavascriptProcessor>>;

	void pushToQueue(const Task::Type& t, JavascriptProcessor* p, const Task::Function& f);

	Result executeNow(const Task::Type& t, JavascriptProcessor* p, const Task::Function& f);

	Result executeQueue(const Task::Type& t, PendingCompilationList& pendingCompilations);

	std::atomic<bool> pending;
	
	bool busy = false;
	Task::Type currentType;

	CriticalSection scriptLock;

	using CompilationTask = SuspendHelpers::Suspended<Task, SuspendHelpers::ScopedTicket>;
	using CallbackTask = SuspendHelpers::Suspended<Task, SuspendHelpers::FreeTicket>;

	using Config = MultithreadedQueueHelpers::Configuration;
	constexpr static Config queueConfig = Config::AllocationsAllowedAndTokenlessUsageAllowed;

	MultithreadedLockfreeQueue<CompilationTask, queueConfig> compilationQueue;
	MultithreadedLockfreeQueue<CallbackTask, queueConfig> lowPriorityQueue;
	MultithreadedLockfreeQueue<CallbackTask, queueConfig> highPriorityQueue;

	MultithreadedLockfreeQueue<WeakReference<ScriptingApi::Content::ScriptPanel>, queueConfig> deferredPanels;
};


} // namespace hise


namespace scriptnode
{
using namespace juce;
using namespace hise;

namespace properties
{

template <class PropertyClass> struct table : public NodePropertyT<int>
{
	table() :
		NodePropertyT<int>(PropertyClass::getId(), -1)
	{};

	template <class RootObject> void initWithRoot(NodeBase* n, RootObject& r)
	{
		if (n != nullptr)
		{
			holder = dynamic_cast<ComplexDataHolder*>(n->getScriptProcessor());

			setAdditionalCallback([&r, this](Identifier id, var newValue)
			{
				if (id == PropertyIds::Value)
				{
					auto index = (int)newValue;
					changed(r, index);
				}
			});

			initialise(n);
		}
	}
	
private:

	template <class RootObject> void changed(RootObject& r, int index)
	{
		if (index == -1)
			usedTable = &ownedTable;
		else
			usedTable = holder->getTable(index);

		if (usedTable == nullptr)
			usedTable = &ownedTable;

		snex::Types::dyn<float> tableData(usedTable->getWritePointer(), usedTable->getTableSize());
		PropertyClass p;
		p.setTableData(r, tableData);
	}

	WeakReference<hise::ComplexDataHolder> holder;
	hise::SampleLookupTable ownedTable;
	WeakReference<hise::Table> usedTable = nullptr;
};



#if 0
template <class PropertyClass> struct slider_pack : public complex_base<PropertyClass>,
													public SliderPack::Listener
{
	// TODO: REWRITE THE FUCKING SLIDER PACK LISTENER CLASS...
	void sliderPackChanged(SliderPack *s, int index) override
	{

	}

	void changed(int index) override
	{
		if (index >= 0)
		{
			if (data = holder->getSliderPackData(index))
			{
				data->addPooledChangeListener(this)
			}
		}
			
		snex::Types::dyn<float> tableData(usedPack->getCachedData());
		PropertyClass p;

		p.setSliderPackData(r, tableData);
	}

	WeakReference<SliderPackData> data;
	HeapBlock<float> dataCopy;
};
#endif



}
}


#endif  // SCRIPTPROCESSOR_H_INCLUDED
