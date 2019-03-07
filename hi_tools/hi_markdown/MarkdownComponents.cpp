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

namespace hise {
using namespace juce;


struct MarkdownHelpButton::MarkdownHelp : public Component
{
	MarkdownHelp(MarkdownRenderer* renderer, int lineWidth)
	{
		setWantsKeyboardFocus(false);

		img = Image(Image::ARGB, lineWidth, (int)renderer->getHeightForWidth((float)lineWidth), true);
		Graphics g(img);

		renderer->draw(g, { 0.0f, 0.0f, (float)img.getWidth(), (float)img.getHeight() });

		setSize(img.getWidth() + 40, img.getHeight() + 40);

	}

	void mouseDown(const MouseEvent& /*e*/) override
	{
		if (auto cb = findParentComponentOfClass<CallOutBox>())
		{
			cb->dismiss();
		}
	}

	void paint(Graphics& g) override
	{
		g.fillAll(Colour(0xFF333333));

		g.drawImageAt(img, 20, 20);
	}

	Image img;
};

MarkdownHelpButton::MarkdownHelpButton() :
	ShapeButton("?", Colours::white.withAlpha(0.7f), Colours::white, Colours::white)
{
	setWantsKeyboardFocus(false);

	setShape(getPath(), false, true, true);

	setSize(16, 16);
	addListener(this);
}



void MarkdownHelpButton::buttonClicked(Button* /*b*/)
{

	if (parser != nullptr)
	{
		if (currentPopup.getComponent())
		{
			currentPopup->dismiss();
		}
		else
		{
			auto nc = new MarkdownHelp(parser, popupWidth);

			auto window = getTopLevelComponent();

			if (window == nullptr)
				return;

			auto lb = window->getLocalArea(this, getLocalBounds());

			if (nc->getHeight() > 700)
			{
				Viewport* viewport = new Viewport();

				viewport->setViewedComponent(nc);
				viewport->setSize(nc->getWidth() + viewport->getScrollBarThickness(), 700);
				viewport->setScrollBarsShown(true, false, true, false);

				currentPopup = &CallOutBox::launchAsynchronously(viewport, lb, window);
				currentPopup->setWantsKeyboardFocus(!ignoreKeyStrokes);
			}
			else
			{
				currentPopup = &CallOutBox::launchAsynchronously(nc, lb, window);
				currentPopup->setWantsKeyboardFocus(!ignoreKeyStrokes);
			}
		}




	}
}


juce::Path MarkdownHelpButton::getPath()
{
	static const unsigned char pathData[] = { 110,109,0,183,97,67,0,111,33,67,98,32,154,84,67,0,111,33,67,128,237,73,67,32,27,44,67,128,237,73,67,0,56,57,67,98,128,237,73,67,224,84,70,67,32,154,84,67,128,1,81,67,0,183,97,67,128,1,81,67,98,224,211,110,67,128,1,81,67,0,128,121,67,224,84,70,67,0,128,
		121,67,0,56,57,67,98,0,128,121,67,32,27,44,67,224,211,110,67,0,111,33,67,0,183,97,67,0,111,33,67,99,109,0,183,97,67,0,111,37,67,98,119,170,108,67,0,111,37,67,0,128,117,67,137,68,46,67,0,128,117,67,0,56,57,67,98,0,128,117,67,119,43,68,67,119,170,108,67,
		128,1,77,67,0,183,97,67,128,1,77,67,98,137,195,86,67,128,1,77,67,128,237,77,67,119,43,68,67,128,237,77,67,0,56,57,67,98,128,237,77,67,137,68,46,67,137,195,86,67,0,111,37,67,0,183,97,67,0,111,37,67,99,109,0,124,101,67,32,207,62,67,108,0,16,94,67,32,207,
		62,67,108,0,16,94,67,32,17,62,67,113,0,16,94,67,32,44,60,67,0,126,94,67,32,0,59,67,113,0,236,94,67,32,207,57,67,0,195,95,67,32,213,56,67,113,0,159,96,67,32,219,55,67,0,151,99,67,32,101,53,67,113,0,44,101,67,32,27,52,67,0,44,101,67,32,8,51,67,113,0,44,
		101,67,32,245,49,67,0,135,100,67,32,95,49,67,113,0,231,99,67,32,196,48,67,0,157,98,67,32,196,48,67,113,0,58,97,67,32,196,48,67,0,79,96,67,32,175,49,67,113,0,105,95,67,32,154,50,67,0,40,95,67,32,227,52,67,108,0,148,87,67,32,243,51,67,113,0,248,87,67,32,
		197,47,67,0,155,90,67,32,59,45,67,113,0,67,93,67,32,172,42,67,0,187,98,67,32,172,42,67,113,0,253,102,67,32,172,42,67,0,155,105,67,32,115,44,67,113,0,41,109,67,32,218,46,67,0,41,109,67,32,219,50,67,113,0,41,109,67,32,132,52,67,0,62,108,67,32,15,54,67,
		113,0,83,107,67,32,154,55,67,0,126,104,67,32,212,57,67,113,0,133,102,67,32,100,59,67,0,254,101,67,32,89,60,67,113,0,124,101,67,32,73,61,67,0,124,101,67,32,207,62,67,99,109,0,207,93,67,32,200,64,67,108,0,194,101,67,32,200,64,67,108,0,194,101,67,32,203,
		71,67,108,0,207,93,67,32,203,71,67,108,0,207,93,67,32,200,64,67,99,101,0,0 };

	Path path;
	path.loadPathFromData(pathData, sizeof(pathData));


	return path;
}


void MarkdownEditor::addPopupMenuItems(PopupMenu& menuToAddTo, const MouseEvent* mouseClickEvent)
{

	menuToAddTo.addItem(AdditionalCommands::LoadFile, "Load file");
	menuToAddTo.addItem(AdditionalCommands::SaveFile, "Save file");
	menuToAddTo.addSeparator();

	CodeEditorComponent::addPopupMenuItems(menuToAddTo, mouseClickEvent);
}

void MarkdownEditor::performPopupMenuAction(int menuItemID)
{
	if (menuItemID == AdditionalCommands::LoadFile)
	{
		FileChooser fc("Load file", File(), "*.md");

		if (fc.browseForFileToOpen())
		{
			currentFile = fc.getResult();

			getDocument().replaceAllContent(currentFile.loadFileAsString());
		}

		return;
	}
	if (menuItemID == AdditionalCommands::SaveFile)
	{
		FileChooser fc("Save file", currentFile, "*.md");

		if (fc.browseForFileToSave(true))
		{
			currentFile = fc.getResult();

			currentFile.replaceWithText(getDocument().getAllContent());
		}

		return;
	}

	CodeEditorComponent::performPopupMenuAction(menuItemID);
}

}