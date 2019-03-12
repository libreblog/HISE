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
*   which must be separately licensed for closed source applications:
*
*   http://www.juce.com
*
*   ===========================================================================
*/

#pragma once

namespace hise {
using namespace juce;


class DocUpdater : public DialogWindowWithBackgroundThread,
				   public MarkdownContentProcessor,
				   public DatabaseCrawler::Logger
{
public:

	DocUpdater(MarkdownDatabaseHolder& holder_, bool fastMode_, bool allowEdit):
		MarkdownContentProcessor(holder_),
		DialogWindowWithBackgroundThread("Update documentation", false),
		holder(holder_),
		crawler(holder),
		fastMode(fastMode_),
		editingShouldBeEnabled(allowEdit)
	{
		holder.addContentProcessor(this);

		if (!fastMode)
		{

			holder.addContentProcessor(&crawler);

			StringArray sa = { "Update local cached file", "Update docs from server" , "Create local HTML offline docs" };

			addComboBox("action", sa, "Action");

			markdownRepository = new FilenameComponent("Markdown Repository", holder.getDatabaseRootDirectory(), false, true, false, {}, {}, "No markdown repository specified");
			markdownRepository->setSize(400, 32);


			htmlDirectory = new FilenameComponent("Target directory", {}, true, true, true, {}, {}, "Select a HTML target directory");
			htmlDirectory->setSize(400, 32);

			htmlDirectory->setEnabled(false);

			addCustomComponent(markdownRepository);

			addCustomComponent(htmlDirectory);

			crawler.setProgressCounter(&getProgressCounter());
			holder.setProgressCounter(&getProgressCounter());

			addBasicComponents(true);
		}
		else
		{
			addBasicComponents(false);
			DialogWindowWithBackgroundThread::runThread();
		}
	}

	~DocUpdater()
	{
		holder.setProgressCounter(nullptr);
		holder.removeContentProcessor(this);
		holder.removeContentProcessor(&crawler);
	}
	

	void logMessage(const String& message) override
	{
		showStatusMessage(message);
	}

	void run() override
	{
		if (fastMode)
		{
			auto mc = dynamic_cast<MainController*>(&holder);
			mc->setAllowFlakyThreading(true);
			holder.setProgressCounter(&getProgressCounter());
			getHolder().setForceCachedDataUse(!editingShouldBeEnabled);
			mc->setAllowFlakyThreading(false);
		}
		else
		{



			auto b = getComboBoxComponent("action");

			auto mc = dynamic_cast<MainController*>(&holder);

			mc->setAllowFlakyThreading(true);

			if (b->getSelectedItemIndex() == 0)
			{
				showStatusMessage("Rebuilding index");
				holder.setForceCachedDataUse(false);

				showStatusMessage("Create Content cache");
				crawler.createContentTree();

				showStatusMessage("Create Image cache");
				crawler.createImageTree();
			}

			mc->setAllowFlakyThreading(false);
		}
	}

	void threadFinished() override
	{
		auto b = getComboBoxComponent("action");

		if (!fastMode && b->getSelectedItemIndex() == 0)
		{
			PresetHandler::showMessageWindow("Cache was updated", "Press OK to rebuild the indexes");
			holder.setForceCachedDataUse(true);
		}

		
	}

	void databaseWasRebuild() override
	{

	}
	
	bool fastMode = false;
	bool editingShouldBeEnabled = false;
	MarkdownDatabaseHolder& holder;
	ScopedPointer<juce::FilenameComponent> markdownRepository;
	ScopedPointer<juce::FilenameComponent> htmlDirectory;
	DatabaseCrawler crawler;
};



class MarkdownPreview : public Component,
					    public MarkdownContentProcessor
{
public:

	enum MouseMode
	{
		Drag,
		Select,
		numMouseModes
	};

	enum EditingMenuCommands
	{
		EditCurrentPage = 1000,
		CreateMarkdownLink,
		CopyLink,
		RevealFile,
		DebugExactContent,
		numEditingMenuCommands
	};

	MarkdownPreview(MarkdownDatabaseHolder& holder);

	~MarkdownPreview();

	void resolversUpdated() override
	{
		renderer.clearResolvers();

		for (auto l : linkResolvers)
			renderer.setLinkResolver(l->clone(&renderer));

		for (auto ip : imageProviders)
			renderer.setImageProvider(ip->clone(&renderer));
	}

	void editCurrentPage(const MarkdownLink& link, bool showExactContent=false);

	void addEditingMenuItems(PopupMenu& m)
	{
		m.addItem(EditingMenuCommands::CopyLink, "Copy link");

		if (editingEnabled)
		{
			m.addSectionHeader("Editing Tools");
			m.addItem(EditingMenuCommands::EditCurrentPage, "Edit this page in new editor tab");
			m.addItem(EditingMenuCommands::CreateMarkdownLink, "Create markdown formatted link", true);
			m.addItem(EditingMenuCommands::RevealFile, "Show file");
			m.addItem(EditingMenuCommands::DebugExactContent, "Debug current content");
		}
	}

	bool performPopupMenuForEditingIcons(int result, MarkdownLink& linkToUse)
	{
		if (result == EditingMenuCommands::EditCurrentPage)
		{
			editCurrentPage(linkToUse);
			return true;
		}
		if (result == EditingMenuCommands::CreateMarkdownLink)
		{
			SystemClipboard::copyTextToClipboard(linkToUse.toString(MarkdownLink::FormattedLinkMarkdown));
			return true;
		}
		if (result == EditingMenuCommands::CopyLink)
		{
			SystemClipboard::copyTextToClipboard(linkToUse.toString(MarkdownLink::Everything));
			return true;
		}
		if (result == EditingMenuCommands::RevealFile)
		{
			auto f = linkToUse.getDirectory({});

			if (f.isDirectory())
			{
				f.revealToUser();
				return true;
			}

			f = linkToUse.getMarkdownFile({});

			if (f.existsAsFile())
			{
				f.revealToUser();
				return true;
			}
		}
		if (result == EditingMenuCommands::DebugExactContent)
		{
			editCurrentPage({}, true);
			return true;
		}

		return false;
	}

	void enableEditing(bool shouldBeEnabled)
	{
		if (editingEnabled != shouldBeEnabled)
		{
			editingEnabled = shouldBeEnabled;

			if (!editingEnabled && PresetHandler::showYesNoWindow("Update local cached documentation", "Do you want to update the local cached documentation from your edited files"))
			{
				auto d = new DocUpdater(getHolder(), false, editingEnabled);
				d->setModalBaseWindowComponent(this);
			}
			else
			{
				auto d = new DocUpdater(getHolder(), true, editingEnabled);
				d->setModalBaseWindowComponent(this);
			}

			

			if (auto ft = findParentComponentOfClass<FloatingTile>())
			{
				ft->getCurrentFloatingPanel()->setCustomTitle(editingEnabled ? "Preview" : "HISE Documentation");

				if (auto c = ft->getParentContainer())
				{
					c->getComponent(0)->getLayoutData().setVisible(editingEnabled);
					c->getComponent(1)->getLayoutData().setVisible(editingEnabled);
					ft->refreshRootLayout();
				}
			}
		}
	}

	bool editingEnabled = true;

	void mouseDown(const MouseEvent& e) override
	{
		if (renderer.navigateFromXButtons(e))
			return;

		if (e.mods.isRightButtonDown())
		{
			PopupLookAndFeel plaf;
			PopupMenu m;
			m.setLookAndFeel(&plaf);

			addEditingMenuItems(m);

			int result = m.show();

			if (performPopupMenuForEditingIcons(result, renderer.getLastLink()))
				return;
		}
	}

	bool keyPressed(const KeyPress& k);

	void setMouseMode(MouseMode newMode)
	{
		if (newMode == Drag)
		{
			viewport.setScrollOnDragEnabled(true);
			internalComponent.enableSelect = false;
		}
		else
		{
			viewport.setScrollOnDragEnabled(false);
			internalComponent.enableSelect = true;
		}
	}

	void setNewText(const String& newText, const File& f);

	struct InternalComponent : public Component,
		public MarkdownRenderer::Listener,
		public juce::SettableTooltipClient
	{
		InternalComponent(MarkdownPreview& parent);

		~InternalComponent();

		int getTextHeight();

		void setNewText(const String& s, const File& f);

		void markdownWasParsed(const Result& r) override;

		void mouseDown(const MouseEvent& e) override;

		void mouseDrag(const MouseEvent& e) override;

		void mouseUp(const MouseEvent& e) override;

		void mouseEnter(const MouseEvent& e) override
		{
			if (enableSelect)
				setMouseCursor(MouseCursor(MouseCursor::IBeamCursor));
			else
				setMouseCursor(MouseCursor(MouseCursor::DraggingHandCursor));
		}

		void mouseExit(const MouseEvent& e) override
		{
			setMouseCursor(MouseCursor::NormalCursor);
		}

		void mouseMove(const MouseEvent& e) override;

		void mouseWheelMove(const MouseEvent& event, const MouseWheelDetails& details) override;

		void scrollToAnchor(float v) override;

		void scrollToSearchResult(Rectangle<float> currentSelection);

		void paint(Graphics & g) override;

		void resized() override
		{
			renderer.updateCreatedComponents();
		}

		MarkdownPreview& parent;
		MarkdownRenderer& renderer;
		
		String errorMessage;

		MarkdownLayout::StyleData styleData;

		Rectangle<float> clickedLink;

		Rectangle<float> currentSearchResult;

		Rectangle<int> currentLasso;
		bool enableSelect = true;
	};


	struct CustomViewport : public ViewportWithScrollCallback
	{
		CustomViewport(MarkdownPreview& parent_) :
			parent(parent_)
		{
			//setScrollOnDragEnabled(true);
		}

		void visibleAreaChanged(const Rectangle<int>& newVisibleArea) override
		{
			auto s = parent.renderer.getAnchorForY(newVisibleArea.getY());
			parent.toc.setCurrentAnchor(s);

			ViewportWithScrollCallback::visibleAreaChanged(newVisibleArea);
		}

		MarkdownPreview& parent;
	};


	struct Topbar : public Component,
		public ButtonListener,
		public LabelListener,
		public TextEditor::Listener,
		public KeyListener
	{

		Topbar(MarkdownPreview& parent_) :
			parent(parent_),
			tocButton("TOC", this, factory),
			homeButton("Home", this, factory),
			backButton("Back", this, factory),
			forwardButton("Forward", this, factory),
			searchPath(factory.createPath("Search")),
			lightSchemeButton("Sun", this, factory, "Night"),
			selectButton("Select", this, factory, "Drag"),
			refreshButton("Rebuild", this, factory),
			editButton("Edit", this, factory, "Lock")
		{
			selectButton.setToggleModeWithColourChange(true);
			editButton.setToggleModeWithColourChange(true);

			addAndMakeVisible(homeButton);
			addAndMakeVisible(tocButton);
			addAndMakeVisible(backButton);
			addAndMakeVisible(forwardButton);
			addAndMakeVisible(lightSchemeButton);
			addAndMakeVisible(searchBar);
			addAndMakeVisible(selectButton);
			addAndMakeVisible(editButton);
			addAndMakeVisible(refreshButton);
			lightSchemeButton.setClickingTogglesState(true);

			const auto& s = parent.internalComponent.styleData;

			searchBar.setColour(Label::backgroundColourId, Colour(0x22000000));
			searchBar.setFont(s.getFont());
			searchBar.setEditable(true);
			searchBar.setColour(Label::textColourId, Colours::white);
			searchBar.setColour(Label::ColourIds::textWhenEditingColourId, Colours::white);
			searchBar.setColour(CaretComponent::ColourIds::caretColourId, Colours::white);
			searchBar.setColour(TextEditor::ColourIds::highlightColourId, Colour(SIGNAL_COLOUR));
			searchBar.setColour(TextEditor::ColourIds::highlightedTextColourId, Colours::black);
			searchBar.setColour(TextEditor::ColourIds::focusedOutlineColourId, Colour(SIGNAL_COLOUR));
			searchBar.addListener(this);
		}


		struct TopbarPaths : public PathFactory
		{
			String getId() const override { return "Markdown Preview"; }

			Path createPath(const String& id) const override;
		};

		MarkdownDataBase* database = nullptr;

		MarkdownPreview& parent;
		TopbarPaths factory;
		HiseShapeButton tocButton;
		HiseShapeButton homeButton;
		HiseShapeButton backButton;
		HiseShapeButton forwardButton;
		HiseShapeButton lightSchemeButton;
		HiseShapeButton selectButton;
		HiseShapeButton refreshButton;
		HiseShapeButton editButton;
		Label searchBar;
		Path searchPath;
	

		struct SearchResults : public Component,
			public Timer,
			public Button::Listener
		{
			struct ItemComponent : public Component
			{
				ItemComponent(MarkdownDataBase::Item i, const MarkdownLayout::StyleData& l) :
					item(i),
					p(i.description),
					style(l)
				{
					p.parse();
					setInterceptsMouseClicks(true, true);
				}

				void mouseEnter(const MouseEvent& e) override
				{
					hover = true;
					setMouseCursor(MouseCursor(MouseCursor::PointingHandCursor));
					repaint();
				}

				void mouseExit(const MouseEvent& e) override
				{
					hover = false;
					setMouseCursor(MouseCursor(MouseCursor::NormalCursor));
					repaint();
				}


				void mouseDown(const MouseEvent& e) override
				{
					down = true;
					repaint();

					if (e.mods.isRightButtonDown())
					{
						PopupLookAndFeel plaf;
						PopupMenu m;
						m.setLookAndFeel(&plaf);

						auto mp = findParentComponentOfClass<MarkdownPreview>();
						mp->addEditingMenuItems(m);

						

						

						int result = m.show();

						if (mp->performPopupMenuForEditingIcons(result, item.url))
							return;

						

					}
				}

				void gotoLink()
				{
					if (auto mp = findParentComponentOfClass<MarkdownPreview>())
					{
						auto& r = mp->renderer;

						r.gotoLink(item.url.withRoot(mp->rootDirectory));

						auto f2 = [mp]()
						{
							mp->currentSearchResults = nullptr;
						};

						MessageManager::callAsync(f2);
					}
				}

				void mouseUp(const MouseEvent& e) override
				{
					

					down = false;
					repaint();

					if(!e.mods.isRightButtonDown())
						gotoLink();
				}

				void paint(Graphics& g) override
				{
					g.fillAll(Colours::grey.withAlpha(down ? 0.6f : (hover ? 0.3f : 0.1f)));

					g.setColour(item.c);

					g.fillRect(0.0f, 0.0f, 3.0f, (float)getHeight());

					auto ar = getLocalBounds();

					auto f = GLOBAL_BOLD_FONT();

					g.setColour(Colours::black.withAlpha(0.1f));
					g.fillRect(kBounds);

					g.setFont(f);
					g.setColour(Colours::white);

					ar.removeFromLeft(kBounds.getWidth());

					g.drawText(item.keywords[0], kBounds.toFloat(), Justification::centred);

					if (!starBounds.isEmpty())
					{
						ar.removeFromLeft(starBounds.getWidth());

						g.setColour(item.c);
						
						Path p;
						p.addStar(starBounds.toFloat().getCentre(), 5, 5.0f, 10.0f);
						g.fillPath(p);
					}

					

					p.draw(g, ar.toFloat().reduced(5.0f).translated(0.0f, -5.0f));

					if (isFuzzyMatch)
						g.fillAll(Colours::grey.withAlpha(0.3f));
				}



				int calculateHeight(int width)
				{
					kBounds = { 0, 0, GLOBAL_BOLD_FONT().getStringWidth(item.keywords[0]) + 20, 0 };

					starBounds = item.type == MarkdownDataBase::Item::Keyword ? Rectangle<int>(kBounds.getRight(), 0, 30, 0) : Rectangle<int>();
					

					if (height == 0)
					{
						height = p.getHeightForWidth((float)(width - 10.0f - kBounds.getWidth() - starBounds.getWidth()));
					}

					kBounds.setHeight(height);
					starBounds.setHeight(height);

					return height;
				}

				bool hover = false;
				bool down = false;

				
				Rectangle<int> kBounds;
				Rectangle<int> starBounds;
				const MarkdownLayout::StyleData& style;
				MarkdownRenderer p;
				int height = 0;
				MarkdownDataBase::Item item;
				bool isFuzzyMatch = false;

				JUCE_DECLARE_WEAK_REFERENCEABLE(ItemComponent);
			};

			SearchResults(Topbar& parent_) :
				parent(parent_),
				nextButton("Forward", this, factory),
				prevButton("Back", this, factory),
				shadower(DropShadow(Colours::black.withAlpha(0.5f), 10, {}))
			{
				addAndMakeVisible(nextButton);
				addAndMakeVisible(prevButton);
				addAndMakeVisible(textSearchResults);
				textSearchResults.setEditable(false);
				textSearchResults.setColour(Label::backgroundColourId, Colours::red.withSaturation(0.3f));
				textSearchResults.setFont(parent.parent.internalComponent.styleData.getFont());
				addAndMakeVisible(viewport);
				viewport.setViewedComponent(&content, false);
				
				shadower.setOwner(this);
			}


			void buttonClicked(Button* b) override
			{
				if (b == &nextButton)
				{
					currentIndex++;

					if (currentIndex >= currentSearchResultPositions.getNumRectangles())
						currentIndex = 0;

					
				}
				if (b == &prevButton)
				{
					currentIndex--;

					if (currentIndex == -1)
						currentIndex = currentSearchResultPositions.getNumRectangles() - 1;
				}

				setSize(getWidth(), 32);

				

				parent.parent.internalComponent.scrollToSearchResult(currentSearchResultPositions.getRectangle(currentIndex));

				refreshTextResultLabel();
			}

				void resized() override
				{
					auto ar = getLocalBounds();

					if (currentSearchResultPositions.isEmpty())
					{
						nextButton.setVisible(false);
						prevButton.setVisible(false);
						textSearchResults.setVisible(false);
					}
					else
					{
						nextButton.setVisible(true);
						prevButton.setVisible(true);
						textSearchResults.setVisible(true);

						auto top = ar.removeFromTop(32);

						nextButton.setBounds(top.removeFromRight(32).reduced(6));
						prevButton.setBounds(top.removeFromRight(32).reduced(6));
						textSearchResults.setBounds(top);
					}
					
					viewport.setBounds(ar);
				}

				void refreshTextResultLabel()
				{
					if (!currentSearchResultPositions.isEmpty())
					{
						String s;

						s << "Search in current page:" << String(currentIndex + 1) << "/" << String(currentSearchResultPositions.getNumRectangles());

						textSearchResults.setText(s, dontSendNotification);
					}
					else
						textSearchResults.setText("No matches", dontSendNotification);
				}

				void timerCallback() override
				{
					currentSearchResultPositions = parent.parent.renderer.searchInContent(searchString);

					refreshTextResultLabel();

					parent.parent.repaint();

					int textSearchOffset = currentSearchResultPositions.isEmpty() ? 0 : 32;

					rebuildItems();

					if (viewport.getViewedComponent()->getHeight() > 350)
					{
						setSize(getWidth(), 350 + textSearchOffset);
					}
					else
					{
						setSize(getWidth(), viewport.getViewedComponent()->getHeight() + textSearchOffset);
					}

					stopTimer();
				}

				void gotoSelection()
				{
					if (currentSelection != nullptr)
					{
						currentSelection->gotoLink();
					}
				}

				void selectNextItem(bool inc)
				{
					if (inc)
					{
						itemIndex++;

						if (itemIndex >= displayedItems.size())
							itemIndex = 0;
					}
					else
					{
						itemIndex--;

						if (itemIndex < 0)
							itemIndex = displayedItems.size();
					}
					
					if (currentSelection = displayedItems[itemIndex])
					{
						for (auto s : displayedItems)
						{
							s->hover = s == currentSelection.get();
							s->repaint();
						}

						auto visibleArea = viewport.getViewArea();

						if (!visibleArea.contains(currentSelection->getPosition()))
						{
							if (currentSelection->getY() > visibleArea.getBottom())
							{
								auto y = currentSelection->getBottom() - visibleArea.getHeight();

								viewport.setViewPosition(0, y);
							}
							else
							{
								viewport.setViewPosition(0, currentSelection->getY());
							}
						}
					}
				}

				void rebuildItems()
				{
					if (parent.database == nullptr)
						return;

					if (searchString.isEmpty())
					{
						displayedItems.clear();
						exactMatches.clear();
						fuzzyMatches.clear();

						content.setSize(viewport.getMaximumVisibleWidth(), 20);
						return;
					}


					auto allItems = parent.database->getFlatList();

					if (searchString.startsWith("/"))
					{
						displayedItems.clear();
						exactMatches.clear();
						fuzzyMatches.clear();

						MarkdownLink linkURL = { parent.parent.rootDirectory, searchString };

						MarkdownDataBase::Item linkItem;

						for (auto item : allItems)
						{
							if (item.url == linkURL)
							{
								linkItem = item;
								break;
							}
						}

						if (linkItem.type != MarkdownDataBase::Item::Invalid)
						{
							ScopedPointer<ItemComponent> newItem(new ItemComponent(linkItem, parent.parent.internalComponent.styleData));

							displayedItems.add(newItem);
							exactMatches.add(newItem);
							content.addAndMakeVisible(newItem.release());
						}
					}
					else
					{
						MarkdownDataBase::Item::PrioritySorter sorter(searchString);

						auto sorted = sorter.sortItems(allItems);

						displayedItems.clear();
						exactMatches.clear();
						fuzzyMatches.clear();

						for (const auto& item : sorted)
						{
							int matchLevel = item.fits(searchString);

							if (matchLevel > 0)
							{
								ScopedPointer<ItemComponent> newItem(new ItemComponent(item, parent.parent.internalComponent.styleData));

								if (matchLevel == 1)
								{
									if(exactMatches.size() < 50)
                                    {
                                        content.addAndMakeVisible(newItem);
                                        exactMatches.add(newItem.release());
                                    }
								}
								else
								{
									if (fuzzyMatches.size() < 10)
									{
										content.addAndMakeVisible(newItem);
										newItem->isFuzzyMatch = true;
										fuzzyMatches.add(newItem.release());
									}
								}
									
							}
						}
                        
                        
					}

                    for(auto i: exactMatches)
                        displayedItems.add(i);
                    
                    for(auto i: fuzzyMatches)
                        displayedItems.add(i);
                    
					content.setSize(viewport.getMaximumVisibleWidth(), 20);

					int y = 0;
					auto w = (float)getWidth();

					for (auto d : displayedItems)
					{
						auto h = d->calculateHeight(content.getWidth());

						d->setBounds(0, y, content.getWidth(), h);
						y += h;

						if (h == 0)
							continue;

						y += 2;

					}

					content.setSize(content.getWidth(), y);
				}

				void setSearchString(const String& s)
				{
					searchString = s;

					startTimer(200);
                    itemIndex = 0;
				}

				void paint(Graphics& g) override
				{
					g.fillAll(Colour(0xFF333333));
					g.fillAll(Colours::black.withAlpha(0.1f));
				}

				

				String searchString;
				Array<ItemComponent*> displayedItems;
				OwnedArray<ItemComponent> exactMatches;
				OwnedArray<ItemComponent> fuzzyMatches;

				TextButton textSearchButton;

				Viewport viewport;
				Component content;

				DropShadower shadower;

				Topbar::TopbarPaths factory;

				HiseShapeButton nextButton;
				HiseShapeButton prevButton;
				Label textSearchResults;
				int currentIndex = -1;
				int itemIndex = 0;
				WeakReference<ItemComponent> currentSelection;
				RectangleList<float> currentSearchResultPositions;

				Topbar& parent;

				String lastText;
				File lastFile;
			};

			void labelTextChanged(Label* labelThatHasChanged) override
			{
				if (labelThatHasChanged->getText().startsWith("/"))
				{
					MarkdownLink l(parent.getHolder().getDatabaseRootDirectory(), labelThatHasChanged->getText());
					parent.renderer.gotoLink(l);
				}
			}

			void textEditorTextChanged(TextEditor& ed)
			{
				if (parent.currentSearchResults != nullptr)
				{
					parent.currentSearchResults->setSearchString(ed.getText());
				}
			}

			void editorShown(Label* l, TextEditor& ed) override
			{
				ed.addListener(this);
				ed.addKeyListener(this);
				if (parent.currentSearchResults == nullptr)
				{
					parent.addAndMakeVisible(parent.currentSearchResults = new SearchResults(*this));

					auto bl = l->getBounds().getBottomLeft();

					auto tl = parent.getLocalPoint(this, bl);

					parent.currentSearchResults->setSize(l->getWidth(), 24);
					parent.currentSearchResults->setTopLeftPosition(tl);
					parent.currentSearchResults->grabKeyboardFocus();
				}
			}

			void textEditorEscapeKeyPressed(TextEditor&)
			{
				parent.currentSearchResults = nullptr;
			}

			void editorHidden(Label*, TextEditor& ed) override
			{
				ed.removeListener(this);
			}

			void updateNavigationButtons()
			{
				//forwardButton.setEnabled(parent.renderer.canNavigate(false));
				//backButton.setEnabled(parent.renderer.canNavigate(true));
			}

			void buttonClicked(Button* b) override
			{
				if (b == &refreshButton)
				{
					parent.renderer.updateHeight();
					parent.internalComponent.repaint();
				}
				if (b == &editButton)
				{
					bool on = b->getToggleState();

					parent.enableEditing(on);

				}
				if (b == &forwardButton)
				{
					parent.renderer.navigate(false);
				}
				if (b == &backButton)
				{
					parent.renderer.navigate(true);
				}
				if (b == &tocButton)
				{
					parent.toc.setVisible(!parent.toc.isVisible());
					parent.resized();
				}
				if (b == &lightSchemeButton)
				{
					if (b->getToggleState())
					{
						MarkdownLayout::StyleData l;
						l.textColour = Colour(0xFF333333);
						l.headlineColour = Colour(0xFF444444);
						l.backgroundColour = Colour(0xFFEEEEEE);
						l.linkColour = Colour(0xFF000044);
						l.codeColour = Colour(0xFF333333);
						parent.internalComponent.styleData = l;

					}
					else
					{
						parent.internalComponent.styleData = {};
					}

					parent.renderer.setStyleData(parent.internalComponent.styleData);

					parent.repaint();

					lightSchemeButton.refreshShape();
				}
				if (b == &selectButton)
				{
					parent.setMouseMode(b->getToggleState() ? Select : Drag);
				}
			}

			bool keyPressed(const KeyPress& key, Component* originatingComponent) override
			{
				if (key == KeyPress::upKey)
				{
					if (parent.currentSearchResults != nullptr)
						parent.currentSearchResults->selectNextItem(false);

					return true;
				}
				else if (key == KeyPress::downKey)
				{
					if (parent.currentSearchResults != nullptr)
						parent.currentSearchResults->selectNextItem(true);

					return true;
				}
				else if (key == KeyPress::returnKey)
				{
					if (searchBar.getText(true).startsWith("/"))
					{

						parent.renderer.gotoLink({ parent.rootDirectory, searchBar.getText(true) });
						searchBar.hideEditor(false);
						parent.currentSearchResults = nullptr;
						return true;
					}

					if (parent.currentSearchResults != nullptr)
						parent.currentSearchResults->gotoSelection();

					return true;
				}
				else if (key == KeyPress::tabKey)
				{

					if (parent.currentSearchResults != nullptr)
						parent.currentSearchResults->nextButton.triggerClick();

					return true;
				}

				return false;

			}

			void resized() override
			{
				const auto& s = parent.internalComponent.styleData;

				Colour c = Colours::white;

				tocButton.setColours(c.withAlpha(0.8f), c, c);

				

				//homeButton.setColours(c.withAlpha(0.8f), c, c);
				//backButton.setColours(c.withAlpha(0.8f), c, c);
				//forwardButton.setColours(c.withAlpha(0.8f), c, c);
				lightSchemeButton.setColours(c.withAlpha(0.8f), c, c);
				selectButton.setColours(c.withAlpha(0.8f), c, c);

				homeButton.setVisible(false);

				auto ar = getLocalBounds();
				int buttonMargin = 12;
				int margin = 0;
				int height = ar.getHeight();

				tocButton.setBounds(ar.removeFromLeft(height).reduced(buttonMargin));
				ar.removeFromLeft(margin);
				refreshButton.setBounds(ar.removeFromLeft(height).reduced(buttonMargin));
				ar.removeFromLeft(margin);
				//homeButton.setBounds(ar.removeFromLeft(height).reduced(buttonMargin));
				backButton.setBounds(ar.removeFromLeft(height).reduced(buttonMargin));
				forwardButton.setBounds(ar.removeFromLeft(height).reduced(buttonMargin));
				ar.removeFromLeft(margin);
				lightSchemeButton.setBounds(ar.removeFromLeft(height).reduced(buttonMargin));
				ar.removeFromLeft(margin);
				selectButton.setBounds(ar.removeFromLeft(height).reduced(buttonMargin));
				ar.removeFromLeft(margin);


				auto delta = 0; //parent.toc.getWidth() - ar.getX();

				ar.removeFromLeft(delta);

				auto sBounds = ar.removeFromLeft(height).reduced(buttonMargin).toFloat();
				searchPath.scaleToFit(sBounds.getX(), sBounds.getY(), sBounds.getWidth(), sBounds.getHeight(), true);

				editButton.setBounds(ar.removeFromRight(height).reduced(buttonMargin));
				
				searchBar.setBounds(ar.reduced(5.0f));
			}

			void paint(Graphics& g) override
			{
				g.fillAll(Colour(0xFF444444));
				//g.fillAll(Colours::white.withAlpha(0.05f));
				g.setColour(Colours::white.withAlpha(0.7f));
				g.fillPath(searchPath);
			}

		};


		class MarkdownDatabaseTreeview : public Component,
										 public MarkdownDatabaseHolder::DatabaseListener
		{
		public:

			struct Item : public juce::TreeViewItem,
				public juce::KeyListener
			{
				Item(MarkdownDataBase::Item item_, MarkdownPreview& previewParent_) :
					TreeViewItem(),
					item(item_),
					previewParent(previewParent_)
				{
					previewParent_.toc.tree.addKeyListener(this);
				}

				~Item()
				{
					previewParent.toc.tree.removeKeyListener(this);
				}

				bool keyPressed(const KeyPress& key,
					Component* originatingComponent) override
				{
					if (key.getKeyCode() == KeyPress::returnKey)
					{
						gotoLink();
						return true;
					}

					return false;
				}

				bool mightContainSubItems() override { return !item.children.isEmpty(); }

				String getUniqueName() const override { return item.url.toString(MarkdownLink::UrlFull); }

				void itemOpennessChanged(bool isNowOpen) override
				{
					if (item.isAlwaysOpen && !isNowOpen)
						return;

					clearSubItems();

					if (isNowOpen)
					{
						for (auto c : item.children)
						{
							if (c.tocString.isEmpty())
								continue;

							auto i = new Item(c, previewParent);

							addSubItem(i);

							auto currentLink = previewParent.renderer.getLastLink();

							const bool open = c.isAlwaysOpen || currentLink.isChildOf(c.url);

							if (open)
								i->setOpen(true);

							
						}
						
					}

					//previewParent.resized();
				}

				MarkdownParser* getCurrentParser()
				{
					return &previewParent.renderer;
				}

				Item* selectIfURLMatches(const MarkdownLink& url)
				{
					if (item.url == url)
					{
						return this;
					}

					for (int i = 0; i < getNumSubItems(); i++)
					{
						if (auto it = dynamic_cast<Item*>(getSubItem(i))->selectIfURLMatches(url))
							return it;
					}

					return nullptr;
				}
			
				void gotoLink()
				{
					if (auto p = getCurrentParser())
					{
						previewParent.currentSearchResults = nullptr;

						previewParent.renderer.gotoLink(item.url.withRoot(previewParent.rootDirectory));

#if 0
						auto link = item.url.upToFirstOccurrenceOf("#", false, false);

						if (p->getLastLink(true, false) != link)
							p->gotoLink(item.url);

						auto anchor = item.url.fromFirstOccurrenceOf("#", true, false);

						if (anchor.isNotEmpty())
						{
							auto mp = &previewParent;

							auto it = this;

							auto f2 = [mp, anchor, it]()
							{
								if (anchor.isNotEmpty())
									mp->renderer.gotoLink(anchor);

								it->setSelected(true, true);
							};

							MessageManager::callAsync(f2);
						}
#endif
					}
				}

				void itemClicked(const MouseEvent& e)
				{
					if (e.mods.isRightButtonDown())
					{
						PopupLookAndFeel plaf;
						PopupMenu m;
						m.setLookAndFeel(&plaf);

						previewParent.addEditingMenuItems(m);

						int result = m.show();

						if (previewParent.performPopupMenuForEditingIcons(result, item.url))
							return;
					}
					else
					{
						gotoLink();
					}

					
				}

			bool canBeSelected() const override
			{
				return true;
			}

			int getItemHeight() const override
			{
				return 26.0f;
			}

			int getItemWidth() const 
			{ 
				auto intendation = getItemPosition(false).getX();
				
				const auto& s = previewParent.internalComponent.styleData;
				auto f = FontHelpers::getFontBoldened(s.getFont().withHeight(16.0f));

				int thisWidth = intendation + f.getStringWidth(item.tocString) + 30.0f;   
				
				int maxWidth = thisWidth;

				for (int i = 0; i < getNumSubItems(); i++)
				{
					maxWidth = jmax<int>(maxWidth, getSubItem(i)->getItemWidth());
				}

				return maxWidth;
			}

			void paintItem(Graphics& g, int width, int height)
			{

				

				Rectangle<float> area({ 0.0f, 0.0f, (float)width, (float)height });

				

				if (isSelected())
				{
					g.setColour(Colours::white.withAlpha(0.3f));
					g.fillRoundedRectangle(area, 2.0f);
				}

				auto r = area.removeFromLeft(3.0f);

				area.removeFromLeft(5.0f);

				const auto& s = previewParent.internalComponent.styleData;

				g.setColour(item.c);
				g.fillRect(r);

				g.setColour(Colours::white.withAlpha(0.8f));

				auto f = FontHelpers::getFontBoldened(s.getFont().withHeight(16.0f));

				g.setFont(f);

				

				if (!item.icon.isEmpty())
				{
					if (auto globalPath = previewParent.getTypedImageProvider<MarkdownParser::GlobalPathProvider>())
					{
						auto img = globalPath->getImage({ previewParent.rootDirectory, item.icon }, (float)height - 4.0f);

						auto pArea = area.removeFromLeft((float)height).reduced(4.0f);

						area.removeFromLeft(5.0f);

						g.drawImageAt(img, pArea.getX(), pArea.getY());
					}

					
				}

				g.drawText(item.tocString, area, Justification::centredLeft);
			}

			MarkdownDataBase::Item item;
			MarkdownPreview& previewParent;
		};

		MarkdownDatabaseTreeview(MarkdownPreview& parent_):
			parent(parent_)
		{
			parent.getHolder().addDatabaseListener(this);
			addAndMakeVisible(tree);

			tree.setColour(TreeView::ColourIds::backgroundColourId, Colour(0xFF222222));
			tree.setColour(TreeView::ColourIds::selectedItemBackgroundColourId, Colours::transparentBlack);
			tree.setColour(TreeView::ColourIds::linesColourId, Colours::red);
			tree.setRootItemVisible(false);

			tree.getViewport()->setScrollBarsShown(true, false);
			databaseWasRebuild();
		}

		~MarkdownDatabaseTreeview()
		{
			parent.getHolder().removeDatabaseListener(this);

			tree.setRootItem(nullptr);
			rootItem = nullptr;
		}

		void scrollToLink(const MarkdownLink& l)
		{
			auto root = tree.getRootItem();

			if (root == nullptr)
				return;

			bool found = false;

			for (int i = 0; i < root->getNumSubItems(); i++)
				found |= closeIfNoMatch(root->getSubItem(i), l);

			if (found)
			{
				if (auto t = dynamic_cast<Item*>(tree.getRootItem())->selectIfURLMatches(l))
				{
					t->setSelected(true, true);
					tree.scrollToKeepItemVisible(t);
				}
			}
		}
		
		void databaseWasRebuild() override
		{
			Component::SafePointer<MarkdownDatabaseTreeview> tmp(this);

			auto f = [tmp]()
			{
				if (tmp.getComponent() == nullptr)
					return;

				auto t = tmp.getComponent();

				t->tree.setRootItem(t->rootItem = new Item(t->parent.getHolder().getDatabase().rootItem, t->parent));
				t->resized();
			};

			MessageManager::callAsync(f);
		}

		void openAll(TreeViewItem* item)
		{
			item->setOpen(true);

			for (int i = 0; i < item->getNumSubItems(); i++)
			{
				openAll(item->getSubItem(i));
			}
		}

		void closeAllExcept(TreeViewItem* item, Array<TreeViewItem*> path)
		{
			if(path.contains(item))
				return;

			item->setOpen(false);
		}

		bool closeIfNoMatch(TreeViewItem* item, const MarkdownLink& id)
		{
			if (dynamic_cast<Item*>(item)->item.url == id)
				return true;

			item->setOpen(true);

			bool found = false;

			for (int i = 0; i < item->getNumSubItems(); i++)
			{
				found |= closeIfNoMatch(item->getSubItem(i), id);
			}

			if (!found)
				item->setOpen(false);

			return found;
		}

		

		void setCurrentAnchor(const String& s)
		{
			if (tree.getRootItem() == nullptr)
				return;

			auto nl = parent.renderer.getLastLink();

			if (auto t = dynamic_cast<Item*>(tree.getRootItem())->selectIfURLMatches(nl.withAnchor(s)))
			{
				t->setSelected(true, true);
				tree.scrollToKeepItemVisible(t);
			}
		}

		int getPreferredWidth() const
		{
			if (rootItem == nullptr)
				return 300;

			return jmax(300, tree.getRootItem()->getItemWidth());
		}

		void paint(Graphics& g) override
		{
			g.fillAll(Colour(0xFF222222));
			
		}

		void resized() override
		{
			tree.setBounds(getLocalBounds());
		}

		juce::TreeView tree;
		ScopedPointer<Item> rootItem;
		MarkdownPreview& parent;
		MarkdownDataBase* db = nullptr;
	};


	void setStyleData(MarkdownLayout::StyleData d)
	{
		internalComponent.styleData = d;
	}

	void paint(Graphics& g) override
	{
		g.fillAll(internalComponent.styleData.backgroundColour);
	}

	void resized() override;

	LookAndFeel_V3 laf;
	
	MarkdownRenderer::LayoutCache layoutCache;
	MarkdownRenderer renderer;

	MarkdownDatabaseTreeview toc;
	CustomViewport viewport;
	InternalComponent internalComponent;
	Topbar topbar;
	File rootDirectory;
	ScopedPointer<Topbar::SearchResults> currentSearchResults;
};


}