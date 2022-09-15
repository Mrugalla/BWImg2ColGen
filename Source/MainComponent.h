#pragma once
#include <JuceHeader.h>
#include <functional>

struct MainComponent :
    public juce::Component,
    public juce::DragAndDropContainer,
    public juce::FileDragAndDropTarget
{
    using Graphics = juce::Graphics;
    using Colour = juce::Colour;
    using Image = juce::Image;
    using DnDSrc = juce::DragAndDropTarget::SourceDetails;
    using StringArray = juce::StringArray;
    using String = juce::String;
    using ImageFileFormat = juce::ImageFileFormat;
    using FileChooser = juce::FileChooser;
    using File = juce::File;
	using uint8 = juce::uint8;
    using FileOutputStream = juce::FileOutputStream;
    using PNGImageFormat = juce::PNGImageFormat;
    using Comp = juce::Component;
    using DragAndDropContainer = juce::DragAndDropContainer;
    using FileDragAndDropTarget = juce::FileDragAndDropTarget;
    using Mouse = juce::MouseEvent;

    static constexpr int PWidth = 9, PHeight = 3;

    MainComponent() :
        Comp(),
        DragAndDropContainer(),
        FileDragAndDropTarget(),

        palette(Image::RGB, PWidth, PHeight, true)
    {
        setOpaque(true);
        setSize(PWidth * 100, PHeight * 100);
    }

    void paint (Graphics& g) override
    {
        g.setImageResamplingQuality(Graphics::lowResamplingQuality);
        g.drawImage(palette, getLocalBounds().toFloat());
    }

    void resized() override
    {
    }

    Image palette;

    bool isInterestedInFileDrag(const StringArray& files) override
    {
		const auto numFiles = files.size();
        if (numFiles != 1)
            return false;

        for (auto f = 0; f < numFiles; ++f)
        {
            auto& file = files[f];
            if (!isImageFile(file))
                return false;
        }
		
        return true;
    }

    void filesDropped(const StringArray& files, int x, int y) override
    {
        {
            auto& file = files[0];
            auto img = ImageFileFormat::loadFrom(file);
            if (!img.isValid())
                return;
            const auto w = img.getWidth();
            const auto h = img.getHeight();

            const auto wA = w / PWidth;
            const auto hA = h / PHeight;

            for (auto y = 0; y < PHeight; ++y)
            {
                const auto yR = static_cast<float>(y) / 3.f;
                const auto yImg = static_cast<int>(yR * h);

                for (auto x = 0; x < PWidth; ++x)
                {
                    const auto xR = static_cast<float>(x) / 9.f;
                    const auto xImg = static_cast<int>(xR * w);

                    auto c = getAverageColour(img, xImg, yImg, wA, hA);
                    palette.setPixelAt(x, y, c);
                }
            }
        }

        maximizeBrightness(palette);

        sortHues(palette);

        const auto loc = getPaletteFile();
        saveImage(palette, loc);

        repaint();
    }

    bool isImageFile(const String& fileName) noexcept
    {
		const auto ext = fileName.fromLastOccurrenceOf(".", false, false);
		return ext == "png" || ext == "jpg" || ext == "jpeg" || ext == "bmp" || ext == "gif";
    }

    Colour getAverageColour(const Image& img, int x, int y, int width, int height) noexcept
    {
		auto r = 0, g = 0, b = 0;
		auto numPixels = 0;

		for (auto y0 = 0; y0 < height; ++y0)
		{
			for (auto x0 = 0; x0 < width; ++x0)
			{
				const auto c = img.getPixelAt(x0 + x, y0 + y);
				r += static_cast<int>(c.getRed());
				g += static_cast<int>(c.getGreen());
				b += static_cast<int>(c.getBlue());
				++numPixels;
			}
		}

		uint8 rgb[3] =
        {
            static_cast<uint8>(r / numPixels),
            static_cast<uint8>(g / numPixels),
            static_cast<uint8>(b / numPixels)
        };

		return Colour::fromRGB(rgb[0], rgb[1], rgb[2]);
    }

    void maximizeBrightness(Image& img)
    {
		const auto w = img.getWidth();
		const auto h = img.getHeight();

        auto maxBrightness = 0.f;

        for (auto y = 0; y < h; ++y)
            for (auto x = 0; x < w; ++x)
            {
                const auto pxl = img.getPixelAt(x, y);
                const auto pb = pxl.getBrightness();
                if (maxBrightness < pb)
                    maxBrightness = pb;
            }

		auto bGain = 1.f / maxBrightness;

		for (auto y = 0; y < h; ++y)
			for (auto x = 0; x < w; ++x)
			{
				auto pxl = img.getPixelAt(x, y);
				pxl = pxl.withBrightness(pxl.getBrightness() * bGain);
				img.setPixelAt(x, y, pxl);
			}
    }

    void maximizeContrast(Image& img)
    {
		const auto w = img.getWidth();
		const auto h = img.getHeight();

		auto minBrightness = 1.f;
		auto maxBrightness = 0.f;

		for (auto y = 0; y < h; ++y)
			for (auto x = 0; x < w; ++x)
			{
				const auto pxl = img.getPixelAt(x, y);
				const auto pb = pxl.getBrightness();
				if (minBrightness > pb)
					minBrightness = pb;
				if (maxBrightness < pb)
					maxBrightness = pb;
			}
		
		const auto bRange = maxBrightness - minBrightness;
        const auto bGain = 1.f / bRange;
		
		for (auto y = 0; y < h; ++y)
			for (auto x = 0; x < w; ++x)
			{
				auto pxl = img.getPixelAt(x, y);
				pxl = pxl.withBrightness((pxl.getBrightness() - minBrightness) * bGain);
				img.setPixelAt(x, y, pxl);
			}
    }

    void sortHues(Image& img)
    {
		const auto w = img.getWidth();
		const auto h = img.getHeight();
		
        for (auto y = 0; y < h; ++y)
		{
            std::vector<Colour> pxls;
            pxls.reserve(w);

			for (auto x = 0; x < w; ++x)
                pxls.emplace_back(img.getPixelAt(x, y));

			std::sort(pxls.begin(), pxls.end(), [](const Colour& a, const Colour& b)
			{
				return a.getBrightness() < b.getBrightness();
			});

			for (auto x = 0; x < w; ++x)
				img.setPixelAt(x, y, pxls[x]);
		}
    }

    void saveImage(const Image& img, const File& file)
    {
        if (file.existsAsFile())
            file.deleteFile();
        file.create();

        FileOutputStream stream(file);
        PNGImageFormat pngWriter;
        pngWriter.writeImageToStream(img, stream);

    }
	
    bool shouldDropFilesWhenDraggedExternally(const DnDSrc& dndSrc,
        StringArray& files, bool& canMoveFiles) override
    {
        return true;
    }

    void mouseDown(const Mouse& mouse) override
    {
        const auto loc = getPaletteFile();

        StringArray files;
        files.add(loc.getFullPathName());

        bool canMoveFiles = true;

        auto callback = []()
        {

        };

        performExternalDragDropOfFiles
        (
            files,
            canMoveFiles,
            this,
            callback
        );
    }

    File getPaletteFile()
    {
        auto loc = File::getSpecialLocation(File::SpecialLocationType::userPicturesDirectory);
        loc = File(loc.getFullPathName() + "/bwImg2ColGenPalette.png");
        return loc;
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainComponent)
};

/*
1. drag n drop image
2. split image into 9x3 pallette
    average colours
3. make palette as bright as possible
4. sort colours
    x-achsis brightness
5. paint colours to screen
6. save to images folder
7. dnd into bitwig
*/