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

MarkdownLink MarkdownLink::createWithoutRoot(const String& validURL)
{
	return { {}, validURL };

#if 0
	MarkdownLink newLink;

	auto withoutExtra = Helpers::removeExtraData(validURL);

	newLink.extraString = Helpers::getExtraData(validURL);
	newLink.anchor = Helpers::getAnchor(withoutExtra);
	newLink.sanitizedURL = Helpers::removeAnchor(withoutExtra);
	newLink.type = Type::Rootless;

	if (newLink.sanitizedURL.startsWith("/images/icon_"))
		newLink.type = Type::Icon;

	return newLink;
#endif
}





File MarkdownLink::toFile(FileType type, File rootToUse) const noexcept
{
	if (file.existsAsFile() && rootToUse == root)
		return file;

	if (!rootToUse.isDirectory())
		rootToUse = root;

	if (!rootToUse.isDirectory())
	{
		// You need to specify the root directory either in the constructor of this link or in the
		// call to this method
		jassertfalse;
	}

	switch (type)
	{
	case FileType::HtmlFile:		return Helpers::createHtmlLink(sanitizedURL, rootToUse.getFullPathName());
	case FileType::ContentFile:	
	{
		auto f = Helpers::getLocalFileForSanitizedURL(rootToUse, sanitizedURL, File::findFiles, "*.md");

		if (f.existsAsFile())
			return f;

		auto asFolder = Helpers::getLocalFileForSanitizedURL(rootToUse, sanitizedURL, File::findDirectories);

		if (asFolder.isDirectory())
			return asFolder.getChildFile("Readme.md");

		return f;
	}
	case FileType::Directory:
	{
		return rootToUse.getChildFile(sanitizedURL.substring(1));
	}
	case FileType::ImageFile:
	{
		return Helpers::getLocalFileForSanitizedURL(rootToUse, sanitizedURL, File::findFiles, "*");
	}
	}

	return {};
}

MarkdownLink::MarkdownLink(const File& rootDirectory, const String& url) :
	root(rootDirectory),
	originalURL(url)
{
	if (url.isEmpty())
	{
		type = Invalid;
	}
	else if (url.startsWith("#"))
	{
		sanitizedURL = "";
		file = {};
		anchor = url;
		type = SimpleAnchor;
	}
	else if (url.startsWith("http"))
	{
		auto httpHeader = url.upToFirstOccurrenceOf(":", true, true);
		auto withoutHeader = url.fromFirstOccurrenceOf(httpHeader, false, false);

		sanitizedURL = httpHeader + Helpers::removeExtraData(withoutHeader);
		extraString = Helpers::getExtraData(withoutHeader);
		type = WebContent;
		file = {};
	}
	else if (url.startsWith("/images/icon_"))
	{
		sanitizedURL = Helpers::getSanitizedURL(Helpers::removeExtraData(url));
		extraString = Helpers::getExtraData(url);
		file = {};
		type = Icon;
	}
	else
	{
		sanitizedURL = Helpers::getSanitizedURL(url);
		anchor = Helpers::getAnchor(sanitizedURL);
		sanitizedURL = Helpers::getSanitizedURL(Helpers::removeAnchor(sanitizedURL));

		if (url.contains(".svg"))
		{
			type = SVGImage;

			if (root.isDirectory())
				file = Helpers::getLocalFileForSanitizedURL(root, sanitizedURL, File::findFiles, "*.svg");

			sanitizedURL = Helpers::removeExtraData(url);
			extraString = Helpers::getExtraData(url);
		}
		else if (Helpers::isImageLink(sanitizedURL))
		{
			type = Image;
			sanitizedURL = Helpers::removeExtraData(url);
			extraString = Helpers::getExtraData(url);

			if (root.isDirectory())
				file = Helpers::getLocalFileForSanitizedURL(root, sanitizedURL, File::findFiles);
		}
		else if (root.isDirectory())
		{
			auto possibleFolder = Helpers::getLocalFileForSanitizedURL(root, sanitizedURL, File::findDirectories);

			if (possibleFolder.isDirectory())
			{
				file = possibleFolder;
				type = Folder;
			}
			else
			{
				file = Helpers::getLocalFileForSanitizedURL(root, sanitizedURL, File::findFiles, "*.md");
				type = Type::MarkdownFile;
			}
		}
		else
		{
			type = MarkdownFileOrFolder;
			file = {};
		}
	}
}

MarkdownLink::MarkdownLink():
	type(Invalid)
{

}

bool MarkdownLink::operator==(const MarkdownLink& other) const
{
	return toString(Everything) == other.toString(Everything);
}

juce::File MarkdownLink::getMarkdownFile(const File& rootDirectory) const noexcept
{
	return toFile(FileType::ContentFile, rootDirectory);
}

MarkdownLink MarkdownLink::withAnchor(const String& newAnchor) const noexcept
{
	String a = newAnchor;
	if (a.isNotEmpty() && !a.startsWith("#"))
		a = "#" + a;

	auto copy = *this;

	copy.anchor = a;
	return copy;
}

juce::File MarkdownLink::getImageFile(const File& rootDirectory) const noexcept
{
	return toFile(FileType::ImageFile, rootDirectory);
}

juce::String MarkdownLink::toString(Format format, const File& rootDirectory) const noexcept
{
	File rootToUse = rootDirectory.isDirectory() ? rootDirectory : root;

	switch (format)
	{
	case Everything:			return sanitizedURL + anchor + (extraString.isEmpty() ? "" : ":" + extraString);
	case UrlFull:				return sanitizedURL + anchor;
	case UrlWithoutAnchor:				return sanitizedURL;
	case AnchorWithHashtag:		return anchor;
	case AnchorWithoutHashtag:	return anchor.substring(1);
	case FormattedLinkHtml:					return Helpers::createHtmlLink(sanitizedURL + anchor, "/");
	case FormattedLinkMarkdown:		return "[" + getNameFromHeader() + "](" + toString(UrlFull) + ")";
	case FormattedLinkMarkdownImage:		return "!" + toString(FormattedLinkMarkdown);
	case FormattedLinkIcon:		return sanitizedURL.fromFirstOccurrenceOf("/images/icon_", false, false);
	case UrlSubPath:				return sanitizedURL.fromLastOccurrenceOf("/", false, false);
	case ContentFull:			return fileExists(rootToUse) ? getMarkdownFile(rootToUse).loadFileAsString().replace("\r\n", "\n") : "";
	case ContentWithoutHeader:	return Helpers::removeMarkdownHeader(toString(ContentFull, rootToUse));
	case ContentHeader:			return Helpers::getMarkdownHeader(toString(ContentFull, rootToUse));
	default:
		break;
	}

	jassertfalse;
	return {};
}

hise::MarkdownLink::Type MarkdownLink::getType() const noexcept
{
	return type;
}

juce::String MarkdownLink::getNameFromHeader() const
{
	String name;

	if (root.isDirectory())
	{
		auto header = getHeaderFromFile(root, false);
		
		name = header.getFirstKeyword();
	}

	if (name.isEmpty())
		name = getPrettyFileName();

	return name;
}

MarkdownLink MarkdownLink::getChildUrl(const String& childName, bool asAnchor) const
{
	return MarkdownLink({}, toString(UrlFull)).getChildUrlWithRoot(childName, asAnchor);
}

MarkdownLink MarkdownLink::getChildUrlWithRoot(const String& childName, bool asAnchor /*= false*/) const
{
	juce_wchar linkChar = (asAnchor ? '#' : '/');

	String s = toString(UrlFull);
	s << linkChar << childName;
	return { root, s };
}

MarkdownLink MarkdownLink::getParentUrl() const
{
	if (type == MarkdownFile)
	{
		if(anchor.isNotEmpty())
			return { root, sanitizedURL };
		else
		{
			auto parentDirectory = file.getParentDirectory();
			return { root, parentDirectory.getRelativePathFrom(root) };
		}
	}
	if (type == Folder)
	{
		auto parentDirectory = file.getParentDirectory();
		return { root, parentDirectory.getRelativePathFrom(root) };
	}
	
	jassertfalse;
	return {};
}

bool MarkdownLink::isChildOf(const MarkdownLink& parent) const
{
	if (parent.getType() == Folder || parent.getType() == MarkdownFileOrFolder)
	{
		return toString(UrlWithoutAnchor).startsWith(parent.toString(UrlWithoutAnchor));
	}

	return false;
}

bool MarkdownLink::isSamePage(const MarkdownLink& otherLink) const
{
	auto thisPage = toString(UrlWithoutAnchor);
	auto otherPage = otherLink.toString(UrlWithoutAnchor);
	
	return thisPage == otherPage;
}

juce::File MarkdownLink::getDirectory(const File& rootDirectory) const noexcept
{
	return toFile(FileType::Directory, rootDirectory);
}

juce::String MarkdownLink::getPrettyFileName() const noexcept
{
	return Helpers::getPrettyName(toString(UrlSubPath));
}

bool MarkdownLink::isImageType() const noexcept
{
	return type == SVGImage || type == Icon || type == Image || type == WebContent;
}

MarkdownLink MarkdownLink::withRoot(const File& rootDirectory) const
{
	return MarkdownLink(rootDirectory, toString(Everything));
}

juce::String MarkdownLink::getExtraData() const noexcept
{
	jassert(isImageType());

	return extraString;
}

hise::MarkdownHeader MarkdownLink::getHeaderFromFile(const File& rootDirectory, bool createIfNonExistent) const
{
	MarkdownParser p(toString(ContentHeader, rootDirectory));
	p.parse();
	return p.getHeader();
}

bool MarkdownLink::fileExists(const File& rootDirectory) const noexcept
{
	if (type == Icon)
		return false;

	return getMarkdownFile(rootDirectory).existsAsFile();
}

double MarkdownLink::Helpers::getSizeFromExtraData(const String& extraData)
{
	if (extraData.contains("%"))
	{
		return extraData.upToFirstOccurrenceOf("%", false, false).getDoubleValue() / -100.0;
	}

	if (extraData.contains("px"))
	{
		return extraData.upToFirstOccurrenceOf("px", false, false).getDoubleValue();
	}

	return extraData.getDoubleValue();
}

juce::String MarkdownLink::Helpers::removeMarkdownHeader(const String& content)
{
	return content.fromLastOccurrenceOf("---\n", false, false);
}

juce::String MarkdownLink::Helpers::getMarkdownHeader(const String& content)
{
	if (content.contains("---"))
	{
		return content.upToLastOccurrenceOf("---\n", true, true);
	}

	return {};
}

}