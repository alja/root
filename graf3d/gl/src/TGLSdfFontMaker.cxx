#include "TGLSdfFontMaker.h"
#include "TGLWidget.h"
#include "TGClient.h"
//#include "TImage.h"
#include "TSystem.h"

#include <cstdio>

#include "TGLSdfFontMakerLowLevel.icxx"

#include "RZip.h"

namespace { // cloned from THttpCallArg::CompressWithGzip()

void gzip_compress_buffer(const char *objbuf, const size_t objlen,
                          std::vector<char> &result)
{
   unsigned long objcrc = R__crc32(0, NULL, 0);
   objcrc = R__crc32(objcrc, (const unsigned char *)objbuf, objlen);
 
   // 10 bytes (ZIP header), compressed data, 8 bytes (CRC and original length)
   int buflen = 10 + objlen + 8;
   if (buflen < 512)
      buflen = 512;
 
   result.resize(buflen);
 
   char *bufcur = result.data();
 
   *bufcur++ = 0x1f; // first byte of ZIP identifier
   *bufcur++ = 0x8b; // second byte of ZIP identifier
   *bufcur++ = 0x08; // compression method
   *bufcur++ = 0x00; // FLAG - empty, no any file names
   *bufcur++ = 0;    // empty timestamp
   *bufcur++ = 0;    //
   *bufcur++ = 0;    //
   *bufcur++ = 0;    //
   *bufcur++ = 0;    // XFL (eXtra FLags)
   *bufcur++ = 3;    // OS   3 means Unix
 
   char dummy[8];
   memcpy(dummy, bufcur - 6, 6);
 
   // R__memcompress fills first 6 bytes with own header, therefore just overwrite them
   unsigned long ziplen = R__memcompress(bufcur - 6, objlen + 6, (char*)objbuf, objlen);
 
   memcpy(bufcur - 6, dummy, 6);
 
   bufcur += (ziplen - 6); // jump over compressed data (6 byte is extra ROOT header)
 
   // write CRC32
   *bufcur++ = objcrc & 0xff;
   *bufcur++ = (objcrc >> 8) & 0xff;
   *bufcur++ = (objcrc >> 16) & 0xff;
   *bufcur++ = (objcrc >> 24) & 0xff;
 
   // write original data length
   *bufcur++ = objlen & 0xff;
   *bufcur++ = (objlen >> 8) & 0xff;
   *bufcur++ = (objlen >> 16) & 0xff;
   *bufcur++ = (objlen >> 24) & 0xff;
 
   result.resize(bufcur - result.data());
   result.shrink_to_fit();
}
}

#include <png.h>

namespace { // adopted from https://gist.github.com/niw/5963798

struct PngWriter
{
    int width = 0;
    int height = 0;
    png_byte color_type = 0;
    png_byte bit_depth = 8;
    std::vector<unsigned char*> row_pointers;

    PngWriter() = default;
    PngWriter(int w, int h, png_byte t = 2, png_byte d = 8) :
        width(w), height(h), color_type(t), bit_depth(d)
    {}

    void set_type(bool is_rgb, bool has_alpha) {
        color_type = is_rgb ? 2 : 0;
        if (has_alpha) color_type |= 4;
    }
    void set_luminance()       { color_type = 0; }
    void set_luminance_alpha() { color_type = 4; }
    void set_rgb()             { color_type = 2; }
    void set_rgba()            { color_type = 6; }

    int write_png_file(std::string_view filename)
    {
        FILE *fp = fopen(filename.data(), "w");
        if(!fp) return(errno);

        png_structp png = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
        if (!png) return(errno);

        png_infop info = png_create_info_struct(png);
        if (!info) return(errno);

        if (setjmp(png_jmpbuf(png))) abort();

        png_init_io(png, fp);

        png_set_IHDR(
            png,
            info,
            width, height,
            bit_depth,
            color_type,
            PNG_INTERLACE_NONE,
            PNG_COMPRESSION_TYPE_DEFAULT,
            PNG_FILTER_TYPE_DEFAULT
        );
        png_write_info(png, info);

        // To remove the alpha channel for PNG_COLOR_TYPE_RGB format,
        // Use png_set_filler().
        // png_set_filler(png, 0, PNG_FILLER_AFTER);

        if (row_pointers.empty()) return 1;

        png_write_image(png, row_pointers.data());
        png_write_end(png, NULL);

        fclose(fp);

        png_destroy_write_struct(&png, &info);

        return 0;
    }
};
}


// struct SdfCreator
// {
//     ...
//     int          max_tex_size = 4096;
//     int          width = 1024;        // atlas image width in pixels
//     int          height = 0;          // atlas image height in pixels (optional, automatic)
//     int          row_height = 96;     // row height in pixels (without SDF border)
//     int          border_size = 16;    // SDF distance in pixels, default 16
//     ...
//     void parse_unicode_ranges( const std::string &nword );
//        e.g.:    'start1:end1,start:end2,single_codepoint' without spaces!
//        default: '31:126,0xffff'
// };


int TGLSdfFontMaker::MakeFont(const char* ttf_font, const char* output_prefix, bool verbose)
{
    if (verbose)
     printf("TGLSdfFontMaker::MakeFont entering.\n");

    const std::string base = "31:126";
    const std::string accented = ",0x00e0:0x00fc,0x010c:0x010d,0x0160:0x0161,0x017d:0x017e";
    const std::string greek = ",0x0391:0x03a9,0x03b1:0x03c9";
    const std::string range_end = ",0xffff";

    root_sdf_fonts::SdfCreator sc;

    // Minimal gl-widget / context.
    std::unique_ptr<TGLWidget> glw(TGLWidget::Create(
        TGLFormat(Rgl::kNone), gClient->GetDefaultRoot(), false, false, nullptr, 1, 1
    ));
    glw->MakeCurrent();

    glGetIntegerv( GL_MAX_RENDERBUFFER_SIZE, &sc.max_tex_size );

    std::string filename     = ttf_font;
    std::string res_filename = output_prefix;

    if ( filename.empty() ) {
        std::cerr << "Input file not specified" << std::endl;
        return( 1 );
    }

    if ( res_filename.empty() ) {
        size_t ext_dot = filename.find_last_of( "." );
        if ( ext_dot == std::string::npos ) {
            res_filename = filename;
        } else {
            res_filename = filename.substr( 0, ext_dot );
        }
    }

    if ( ! sc.font.load_ttf_file( filename.c_str() ) ) {
        std::cerr << "Error reading TTF file '" << filename << "' " << std::endl;
        return( 1 );
    }

    // Allocating glyph rects

    sc.sdf_atlas.init( &sc.font, sc.width, sc.row_height, sc.border_size );

    sc.parse_unicode_ranges(base + accented + greek + range_end);
    sc.apply_unicode_ranges();

    sc.sdf_atlas.draw_glyphs( sc.gp );

    if (verbose) {
        std::cout << "Allocated " << sc.sdf_atlas.glyph_count << " glyphs\n";
        std::cout << "Atlas maximum height is " << sc.sdf_atlas.max_height << "\n";
    }

    if ( sc.height == 0 ) {
        sc.height = sc.sdf_atlas.max_height;
    }

    // GL initialization
    
    sc.sdf_gl.init();    

    GLuint rbcolor;
    glGenRenderbuffers( 1, &rbcolor );
    glBindRenderbuffer( GL_RENDERBUFFER, rbcolor );
    glRenderbufferStorage( GL_RENDERBUFFER, GL_RED, sc.width, sc.height );
    glBindRenderbuffer( GL_RENDERBUFFER, 0 );

    GLuint rbds;
    glGenRenderbuffers( 1, &rbds );
    glBindRenderbuffer( GL_RENDERBUFFER, rbds );
    glRenderbufferStorage( GL_RENDERBUFFER, GL_DEPTH_STENCIL, sc.width, sc.height );
    glBindRenderbuffer( GL_RENDERBUFFER, 0 );

    GLuint fbo;
    glGenFramebuffers( 1, &fbo );
    glBindFramebuffer( GL_FRAMEBUFFER, fbo );
    glFramebufferRenderbuffer( GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, rbcolor );
    glFramebufferRenderbuffer( GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, rbds );

    if ( glCheckFramebufferStatus( GL_FRAMEBUFFER ) != GL_FRAMEBUFFER_COMPLETE ) {
        std::cerr << "Error creating framebuffer!" << std::endl;
        return( 1 );
    }

    // Rendering glyphs

    uint8_t* picbuf = (uint8_t*) malloc( sc.width * sc.height );

    glViewport( 0, 0, sc.width, sc.height );
    glClearColor( 0.0, 0.0, 0.0, 0.0 );
    glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT );

    sc.sdf_gl.render_sdf( { float(sc.width), float(sc.height) }, sc.gp.fp.vertices, sc.gp.lp.vertices );

    glReadPixels( 0, 0, sc.width, sc.height, GL_RED, GL_UNSIGNED_BYTE, picbuf );

    glBindFramebuffer( GL_FRAMEBUFFER, 0 );
    glFinish();

    if (verbose)
        printf("Resulting GL buffer: w=%d, h=%d\n", sc.width, sc.height);

    PngWriter pw(sc.width, sc.height, 0, 8);
    pw.row_pointers.resize(sc.height);
    for ( int iy = 0; iy < sc.height; ++iy ) {
        pw.row_pointers[sc.height - iy - 1] = picbuf + iy * sc.width;
    }

    pw.write_png_file(res_filename + ".png");

    free( picbuf );

    // Saving JSON

    std::string json = sc.sdf_atlas.json( sc.height );
    std::vector<char> json_gz;
    gzip_compress_buffer(json.data(), json.size(), json_gz);

    std::string json_filename = res_filename + ".js.gz";
    FILE *json_file = fopen(json_filename.c_str(), "w");
    if ( !json_file ) {
        perror("Error writing json file.");
        return errno;
    }
    fwrite(json_gz.data(), json_gz.size(), 1, json_file);
    fclose(json_file);

    return 0;
}
