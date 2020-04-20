#include <mitsuba/core/bitmap.h>
#include <mitsuba/core/filesystem.h>
#include <mitsuba/core/fstream.h>
#include <mitsuba/core/spectrum.h>
#include <mitsuba/core/string.h>
#include <mitsuba/render/film.h>
#include <mitsuba/render/fwd.h>
#include <mitsuba/render/imageblock.h>

NAMESPACE_BEGIN(mitsuba)

/**!

.. _film-TiledHDRFilm:

High dynamic range film (:monosp:`TiledHDRFilm`)
-------------------------------------------

.. pluginparameters::

 * - width, height
   - |int|
   - Width and height of the camera sensor in pixels Default: 768, 576)
 * - pixel_format
   - |string|
   - Specifies the desired pixel format of output images. The options are :monosp:`luminance`,
     :monosp:`luminance_alpha`, :monosp:`rgb`, :monosp:`rgba`, :monosp:`xyz` and :monosp:`xyza`.
     (Default: :monosp:`rgba`)
 * - component_format
   - |string|
   - Specifies the desired floating  point component format of output images. The options are
     :monosp:`float16`, :monosp:`float32`, or :monosp:`uint32`. (Default: :monosp:`float16`)
 * - crop_offset_y, crop_offset_y, crop_width, crop_height
   - |int|
   - These parameters can optionally be provided to select a sub-rectangle
     of the output. In this case, only the requested regions
     will be rendered. (Default: Unused)
 * - (Nested plugin)
   - :paramtype:`rfilter`
   - Reconstruction filter that should be used by the film. (Default: :monosp:`gaussian`, a windowed
     Gaussian filter)

 */

template <typename Float, typename Spectrum>
class TiledHDRFilm final : public Film<Float, Spectrum> {
public:
    MTS_IMPORT_BASE(Film, m_size, m_crop_size, m_crop_offset, m_filter)
    MTS_IMPORT_TYPES(ImageBlock)

    TiledHDRFilm(const Properties &props) : Base(props) {
        std::string pixel_format = string::to_lower(
            props.string("pixel_format", "rgba"));
        std::string component_format = string::to_lower(
            props.string("component_format", "float16"));
	m_block_size = props.float32("block_size", 32);

        m_dest_file = props.string("filename", "");

        if (pixel_format == "luminance" || is_monochromatic_v<Spectrum>) {
            m_pixel_format = Bitmap::PixelFormat::Y;
            if (pixel_format != "luminance")
                Log(Warn,
                    "Monochrome mode enabled, setting film output pixel format "
                    "to 'luminance' (was %s).",
                    pixel_format);
        } else if (pixel_format == "luminance_alpha")
            m_pixel_format = Bitmap::PixelFormat::YA;
        else if (pixel_format == "rgb")
            m_pixel_format = Bitmap::PixelFormat::RGB;
        else if (pixel_format == "rgba")
            m_pixel_format = Bitmap::PixelFormat::RGBA;
        else if (pixel_format == "xyz")
            m_pixel_format = Bitmap::PixelFormat::XYZ;
        else if (pixel_format == "xyza")
            m_pixel_format = Bitmap::PixelFormat::XYZA;
        else {
            Throw("The \"pixel_format\" parameter must either be equal to "
                  "\"luminance\", \"luminance_alpha\", \"rgb\", \"rgba\", "
                  " \"xyz\", \"xyza\". Found %s.",
                  pixel_format);
        }

        if (component_format == "float16")
            m_component_format = Struct::Type::Float16;
        else if (component_format == "float32")
            m_component_format = Struct::Type::Float32;
        else if (component_format == "uint32")
            m_component_format = Struct::Type::UInt32;
        else {
            Throw("The \"component_format\" parameter must either be "
                  "equal to \"float16\", \"float32\", or \"uint32\"."
                  " Found %s instead.", component_format);
        }
    }

    void set_destination_file(const fs::path &dest_file) override {
        m_dest_file = dest_file;
        std::string extension = string::to_lower(m_dest_file.extension().string());
        if(extension != ".exr")
            m_dest_file.replace_extension(".exr");

        Log(Info, "Commencing creation of a tiled EXR image at \"%s\" ..", m_dest_file.string().c_str());
        
        Imf::Header header(m_size.x, m_size.y);
        header.setTileDescription(Imf::TileDescription(m_block_size, m_block_size, Imf::ONE_LEVEL));
        header.insert("generated-by", Imf::StringAttribute("Mitsuba version " MTS_VERSION));

        if (m_pixelFormats.size() == 1) {
            /* Write a chromaticity tag when this is possible */
            Bitmap::EPixelFormat pixelFormat = m_pixelFormats[0];
            if (pixelFormat == Bitmap::EXYZ || pixelFormat == Bitmap::EXYZA) {
                Imf::addChromaticities(header, Imf::Chromaticities(
                    Imath::V2f(1.0f, 0.0f),
                    Imath::V2f(0.0f, 1.0f),
                    Imath::V2f(0.0f, 0.0f),
                    Imath::V2f(1.0f/3.0f, 1.0f/3.0f)));
            } else if (pixelFormat == Bitmap::ERGB || pixelFormat == Bitmap::ERGBA) {
                Imf::addChromaticities(header, Imf::Chromaticities());
            }
        }

        Imf::PixelType compType;
        size_t compStride;

        if (m_componentFormat == Bitmap::EFloat16) {
            compType = Imf::HALF;
            compStride = 2;
        } else if (m_componentFormat == Bitmap::EFloat32) {
            compType = Imf::FLOAT;
            compStride = 4;
        } else if (m_componentFormat == Bitmap::EUInt32) {
            compType = Imf::UINT;
            compStride = 4;
        } else {
            Log(Error, "Invalid component type (must be "
                "float16, float32, or uint32)");
            return;
        }

        Imf::ChannelList &channels = header.channels();
        for (size_t i=0; i<m_channelNames.size(); ++i)
            channels.insert(m_channelNames[i].c_str(), Imf::Channel(compType));

        m_output = new Imf::TiledOutputFile(m_dest_file.string().c_str(), header);
        m_frame_buffer = new Imf::FrameBuffer();
        m_block_size = (int) blockSize;
        m_blocksH = (m_size.x + blockSize - 1) / blockSize;
        m_blocksV = (m_size.y + blockSize - 1) / blockSize;

        m_pixel_stride = m_channelNames.size() * compStride;
        m_row_stride = m_pixel_stride * m_block_size;

        if (m_pixelFormats.size() == 1) {
            m_tile = new Bitmap(m_pixelFormats[0], m_componentFormat,
                    Vector2i(m_block_size, m_block_size));
        } else {
            m_tile = new Bitmap(Bitmap::EMultiChannel, m_componentFormat,
                    Vector2i(m_block_size, m_block_size), (uint8_t) m_channelNames.size());
            m_tile->setChannelNames(m_channelNames);
        }

        char *ptr = (char *) m_tile->getUInt8Data();

        for (size_t i=0; i<m_channelNames.size(); ++i) {
            m_frame_buffer->insert(m_channelNames[i].c_str(),
                Imf::Slice(compType, ptr, m_pixel_stride, m_row_stride));
            ptr += compStride;
        }

        m_output->setFrameBuffer(*m_frame_buffer);
        m_peak_usage = 0;
    }

    void prepare(const std::vector<std::string> &channels) override {
        std::vector<std::string> channels_sorted = channels;
        channels_sorted.push_back("R");
        channels_sorted.push_back("G");
        channels_sorted.push_back("B");
        std::sort(channels_sorted.begin(), channels_sorted.end());
        for (size_t i = 1; i < channels.size(); ++i) {
            if (channels[i] == channels[i - 1])
                Throw("Film::prepare(): duplicate channel name \"%s\"", channels[i]);
        }

        m_channels = channels;
    }

    void put(const ImageBlock *block) override {
        Assert(m_output != NULL);

        if ((block->offset().x() % m_block_size) != 0 ||
            (block->offset().y() % m_block_size) != 0)
            Log(Error, "Encountered an unaligned block!");

        if (block->size().x() > m_block_size ||
            block->size().y() > m_block_size)
            Log(Error, "Encountered an oversized block!");

        int x = block->offset().x() / (int) m_block_size;
        int y = block->offset().y() / (int) m_block_size;

        /* Create two copies: a clean one, and one that is used for accumulation */
        ref<ImageBlock> copy1, copy2;
        if (m_free_blocks.size() > 0) {
            copy1 = m_free_blocks.back();
            block->copy_to(copy1);
            m_free_blocks.pop_back();
        } else {
            copy1 = block->clone();
            copy1->inc_ref();
            ++m_peakUsage;
        }

        if (m_free_blocks.size() > 0) {
            copy2 = m_free_blocks.back();
            block->copy_to(copy2);
            m_free_blocks.pop_back();
        } else {
            copy2 = block->clone();
            copy2->inc_ref();
            ++m_peakUsage;
        }

        uint32_t idx = (uint32_t) x + (uint32_t) y * m_blocksH;
        m_orig_blocks[idx]   = copy1;
        m_merged_blocks[idx] = copy2;

        for (int yo = -1; yo <= 1; ++yo)
            for (int xo = -1; xo <= 1; ++xo)
                potentially_write(x + xo, y + yo);
    }

    void potentially_write(int x, int y) {
        if (x < 0 || y < 0 || x >= m_blocksH || y >= m_blocksV)
            return;

        uint32_t idx = (uint32_t) x + (uint32_t) y * m_blocksH;
        std::map<uint32_t, ImageBlock *>::iterator it = m_orig_blocks.find(idx);
        if (it == m_orig_blocks.end())
            return;

        ImageBlock *orig_block = it->second;
        if (orig_block == NULL)
            return;

        /* This could be accelerated using some counters */
        for (int yo = -1; yo <= 1; ++yo) {
            for (int xo = -1; xo <= 1; ++xo) {
                int xp = x + xo, yp = y + yo;
                if (xp < 0 || yp < 0 || xp >= m_blocksH || yp >= m_blocksV
                   || (xp == x && yp == y))
                    continue;

                uint32_t idx2 = (uint32_t) xp + (uint32_t) yp * m_blocksH;
                if (m_orig_blocks.find(idx2) == m_orig_blocks.end())
                    return; /* Not all neighboring blocks are there yet */
            }
        }

        ImageBlock *merged_block = m_merged_blocks[idx];
        if (merged_block == NULL)
            return;

        /* All neighboring blocks are there -- join overlapping regions */
        for (int yo = -1; yo <= 1; ++yo) {
            for (int xo = -1; xo <= 1; ++xo) {
                int xp = x + xo, yp = y + yo;
                if (xp < 0 || yp < 0 || xp >= m_blocksH || yp >= m_blocksV
                   || (xp == x && yp == y))
                    continue;
                uint32_t idx2 = (uint32_t) xp + (uint32_t) yp * m_blocksH;
                ImageBlock *orig_block2   = m_orig_blocks[idx2];
                ImageBlock *merged_block2 = m_merged_blocks[idx2];
                if (!orig_block2 || !merged_block2)
                    continue;

                merged_block->put(orig_block2);
                merged_block2->put(orig_block);
            }
        }

        if constexpr (is_cuda_array_v<Float>) {
            cuda_eval();
            cuda_sync();
        }

        // const Bitmap *source = merged_block->getBitmap();
        ref<Bitmap> source = new Bitmap(m_channels.size() != 5 ? Bitmap::PixelFormat::MultiChannel
                                                               : Bitmap::PixelFormat::XYZAW,
                          struct_type_v<ScalarFloat>, merged_block->size(), merged_block->channel_count(),
                          (uint8_t *) merged_block->data().managed().data());

        size_t sourceBpp = source->bytes_per_pixel();
        size_t targetBpp = m_tile->bytes_per_pixel();

        const uint8_t *sourceData = source->uint8_data()
            + merged_block->border_size() * sourceBpp * (1 + source->width());
        uint8_t *targetData = m_tile->uint8_data();

        


        /* Commit to disk */
        size_t ptrOffset = merged_block->offset().x() * m_pixel_stride +
            merged_block->offset().y() * m_rowStride;

        for (Imf::FrameBuffer::Iterator it = m_frameBuffer->begin();
            it != m_frameBuffer->end(); ++it)
            it.slice().base -= ptrOffset;

        m_output->setFrameBuffer(*m_frameBuffer);
        m_output->writeTile(x, y);

        for (Imf::FrameBuffer::Iterator it = m_frameBuffer->begin();
            it != m_frameBuffer->end(); ++it)
            it.slice().base += ptrOffset;

        /* Release the block */
        m_free_blocks.push_back(orig_block);
        m_free_blocks.push_back(merged_block);
        m_orig_blocks[idx] = NULL;
        m_merged_blocks[idx] = NULL;
    }

    bool develop(const ScalarPoint2i  &source_offset,
                 const ScalarVector2i &size,
                 const ScalarPoint2i  &taroffset,
                 Bitmap *target) const override {
        Log(Error, "Not Implemented for TiledHDRFilm");
        return false;
    }

    ref<Bitmap> bitmap(bool raw = false) override {
        Log(Error, "Not Implemented for TiledHDRFilm");
     };

    void develop() override {
        if (m_output) {
            Log(Info, "Closing EXR file (%u tiles in total, peak memory usage: %u tiles)..",
                m_blocksH * m_blocksV, m_peak_usage);
            delete m_output;
            delete m_frame_buffer;
            m_output = NULL;
            m_frame_buffer = NULL;
            m_tile = NULL;

            //for (std::vector<ImageBlock *>::iterator it = m_free_blocks.begin();
                //it != m_free_blocks.end(); ++it)
             for(auto it : m_free_blocks)
		   it->dec_ref();
            m_free_blocks.clear();

            //for (std::map<uint32_t, ImageBlock *>::iterator it = m_orig_blocks.begin();
              //  it != m_orig_blocks.end(); ++it) {
                for(auto it : m_orig_blocks) {
		if (it.second)
                    it.second->dec_ref();
            }
            m_orig_blocks.clear();

            for (std::map<uint32_t, ImageBlock *>::iterator it = m_merged_blocks.begin();
                it != m_merged_blocks.end(); ++it) {
                if ((*it).second)
                    (*it).second->decRef();
            }
            m_merged_blocks.clear();
        }
    }

    bool destination_exists(const fs::path &base_name) const override {
        fs::path filename = base_name;
        if (string::to_lower(filename.extension().string()) != ".exr")
            filename.replace_extension(".exr");
        return fs::exists(filename);
    }

    std::string to_string() const override {
        std::ostringstream oss;
        oss << "TiledHDRFilm[" << std::endl
            << "  size = " << m_size        << "," << std::endl
            << "  crop_size = " << m_crop_size   << "," << std::endl
            << "  crop_offset = " << m_crop_offset << "," << std::endl
            << "  filter = " << m_filter << "," << std::endl
            << "  pixel_format = " << m_pixel_format << "," << std::endl
            << "  component_format = " << m_component_format << "," << std::endl
            << "  dest_file = \"" << m_dest_file << "\"" << std::endl
            << "]";
        return oss.str();
    }

    MTS_DECLARE_CLASS()
protected:
    Bitmap::PixelFormat m_pixel_format;
    Struct::Type m_component_format;
    fs::path m_dest_file;
    std::vector<std::string> m_channels;

    std::vector<ImageBlock *> m_free_blocks;
    std::map<uint32_t, ImageBlock *> m_orig_blocks, m_merged_blocks;
    Imf::TiledOutputFile *m_output;
    Imf::FrameBuffer *m_frame_buffer;
    ref<Bitmap> m_tile;
    size_t m_pixel_stride, m_row_stride;
    int m_blocksH, m_blocksV, m_peak_usage;
    int m_block_size;
};

MTS_IMPLEMENT_CLASS_VARIANT(TiledHDRFilm, Film)
MTS_EXPORT_PLUGIN(TiledHDRFilm, "HDR Film")
NAMESPACE_END(mitsuba)
