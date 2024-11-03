// This utility converts GIMP images in C source code header format to a format
// suitable for display on a KIM-1 with one to four K-1008 cards as described in
// the "Use of the K-1008 for grey scale display, app note #2" document.
//
// (C) 2024 Eduardo Casino under the terms of the General Public License, Version 2
//
// https://github.com/eduardocasino/k-1008-multiple-cards-image-util
//
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <libgen.h>
#include <math.h>
#include <ctype.h>
#include <errno.h>

#include "ihex.h"
#include "pap.h"

#define MAX_PALETTE_SIZE 16
#define MAX_COL_BYTES 40
#define MAX_ROWS 200
#define MAX_IMAGE_SIZE (MAX_COL_BYTES*8*MAX_ROWS)
#define MAX_CARDS 4
#define CARD_MEMORY_SIZE 8192
#define MIN_BASE_ADDRESS 0x2000
#define MAX_BASE_ADDRESS 0xA000
#define DEFAULT_BASE_ADDRESS MIN_BASE_ADDRESS

typedef bool (*output_fn_t)();

typedef struct { 
    char *format_string;
    char *format_des;
    output_fn_t output_fn;
} formats_t; 

typedef struct {
    uint16_t base_address;
    char *input_filename;
    char *output_filename;
    char *palette_filename;
    const formats_t *format;
} options_t;

typedef struct {
    uint8_t r;
    uint8_t g;
    uint8_t b;
} color_t;

bool output_binary( uint8_t *data, options_t *options, int data_size, int color_bits, uint16_t x_size, uint16_t y_size );
bool output_pap( uint8_t *data, options_t *options, int data_size, int color_bits, uint16_t x_size, uint16_t y_size );
bool output_ihex( uint8_t *data, options_t *options, int data_size, int color_bits, uint16_t x_size, uint16_t y_size );
bool output_asm( uint8_t *data, options_t *options, int data_size, int color_bits, uint16_t x_size, uint16_t y_size );

static const formats_t formats[] = {
    { "pap", "MOS Papertape (default)", (output_fn_t) output_pap },
    { "ihex", "Intel HEX", (output_fn_t) output_ihex },
    { "asm", "CA65 assembly code", (output_fn_t) output_asm },
#if 0
    { "bin", "Binary output", (output_fn_t) output_binary },
#endif
    { NULL }
};

FILE *open_palette( char *file_name, char *buffer )
{
    static const char palette_sig[] = "GIMP Palette\n";
    FILE *palette_f = NULL;
    
    palette_f = fopen( file_name, "r" );

    if ( NULL != palette_f )
    {
        size_t nbytes = fread( buffer, sizeof( palette_sig ) - 1, 1, palette_f );

        if ( strncmp( buffer, palette_sig, sizeof( palette_sig ) ) )
        {
            fputs( "Unknown palette file format\n", stderr );
            fclose( palette_f );
            palette_f = NULL;
        }
    }
    else
    {
        perror( "Error opening palette file" );
    }

    return palette_f;
}

int read_palette( char *file_name, char *buffer, size_t bufsiz, color_t *palette )
{
    const char *palette_s = "%hhu %hhu %hhu %*s";

    FILE *palette_f;
    int ncolors = 0;

    if ( NULL != ( palette_f = open_palette( file_name, buffer ) ) )
    {
        char *line;
        int matches;

        while ( NULL != ( line = fgets( buffer, bufsiz, palette_f ) ) )
        {
            matches = sscanf( line, "%hhu %hhu %hhu %*s", &palette[ncolors].r, &palette[ncolors].g, &palette[ncolors].b );

            if ( matches > 0 )
            {

                if ( matches != 3 )          
                {
                    // Should not occur
                    fputs( "Bad palette file\n", stderr );
                    ncolors = 0;
                    break;
                }
                else
                {

                    if ( ncolors == MAX_PALETTE_SIZE )
                    {
                        fprintf( stderr, "Too many colors (max. is %d)\n", ncolors);
                        ncolors = 0;
                        break;
                    }

                    ++ncolors;
                }
            }
        }

        fclose( palette_f );
    }

    return ncolors;
}

bool get_image_dimensions( FILE *image_file, char *buffer, size_t bufsiz, uint16_t *x_size, uint16_t *y_size )
{
    static const char x_size_s[] = "static unsigned int width = %hu;";
    static const char y_size_s[] = "static unsigned int height = %hu;";

    char *line;

    *x_size = 0;
    *y_size = 0;

    while ( NULL != ( line = fgets( buffer, bufsiz, image_file ) ) )
    {
        sscanf( line, x_size_s, x_size );
        sscanf( line, y_size_s, y_size );

        if ( *x_size && *y_size )
        {
            return true;
        }
    }

    return false;
}

int translate_cmap( FILE *image_file, char *buffer, size_t bufsiz, color_t *palette, uint8_t *cmap, int ncolors )
{
    char *line;
    int image_colors = 0;
    int start = 0;

    while ( NULL != ( line = fgets( buffer, bufsiz, image_file ) ) )
    {
        int c;
        int matches;
        uint8_t r, g, b;

        matches = sscanf( line, " { %hhu , %hhu , %hhu } ,", &r, &g, &b );

        if ( matches > 0 )
        {
            if ( matches != 3 )          
            {
                image_colors = 0;
                break;
            }
            else
            {
                ++start;

                for ( c = 0; c < ncolors; ++c )
                {
                    if ( palette[c].r == r && palette[c].g == g && palette[c].b == b )
                    {
                        break;
                    }
                }

                if ( c == ncolors )
                {
                    image_colors = 0;
                    break;
                }
            
                cmap[image_colors++] = c;

                if ( image_colors == ncolors )
                {
                    break;
                }

            }
        }
        else
        {
            if ( start )
            {
                image_colors = 0;
                break;
            }
        }
    }

    return image_colors;
}

bool search_for_header_data( FILE *image_file, char *buffer, size_t bufsiz )
{
    static const char header_s[] = "static unsigned char header_data[] = {\n";
    char *line;

    while ( NULL != ( line = fgets( buffer, bufsiz, image_file ) ) )
    {
        if ( !strncmp( line, header_s, sizeof( header_s ) ) )
        {
            return true;
        }
    }

    return false;
}

int parse_image( FILE *image_file, char *buffer, size_t bufsiz, uint8_t *image, uint8_t *cmap )
{
    int image_size = 0;
    char *line;

    while ( NULL != ( line = fgets( buffer, bufsiz, image_file ) ) )
    {
        if ( strstr( line, "};" ) )
        {
            return image_size;
        }

        while ( *line != '\n' && line != buffer + BUFSIZ )
        {
            if ( !isdigit( *line ) )
            {
                ++line;
                continue;
            }

            if ( image_size > MAX_IMAGE_SIZE )
            {
                fputs( "Error: Image is too big.\n", stderr );
                return 0;
            }

            uint8_t color = (uint8_t)strtoul(line, &line, 10 );

            if ( errno )
            {
                perror( "Error: Bad image data format" );
                return 0;
            }

            image[image_size++] = cmap[color];

        }

        if ( *line != '\n' )
        {
            fputs( "Error: Bad image data format\n", stderr );
            return 0;
        }

    }

    fputs( "Error: Can't find image data end\n", stderr );
    return 0;
}

int convert_to_layers( uint8_t *raw, uint8_t *binary, int color_bits, uint16_t x_size, uint16_t y_size )
{

    int conv_byte = 0, data_size = 0;

    printf( "Color bits: %d\n", color_bits );
    
    for ( uint16_t y = 0; y < y_size; ++y )
    {
        for ( uint16_t x = 0; x < x_size; x += 8 )
        {
            for ( int cbit = 0; cbit < color_bits; ++cbit )
            {
                binary[conv_byte+CARD_MEMORY_SIZE*cbit] = 0;

                for ( int pixel = 0; pixel < 8 && (x + pixel) < x_size; ++pixel )
                {
                    binary[conv_byte+CARD_MEMORY_SIZE*cbit] |= ((raw[(y*x_size)+x+pixel] >> cbit) & 1) << (7 - pixel);
                }
                ++data_size;
            }
            ++conv_byte;
        }
    }

    return data_size;
}

#define BYTES_PER_LINE 16
bool output_asm( uint8_t *data, options_t *options, int data_size, int color_bits, uint16_t x_size, uint16_t y_size )
{
    FILE *output_file = fopen( options->output_filename, "w" );

    if ( NULL == output_file )
    {
        perror( "Error opening output file" );
        return false;
    }

    fprintf( output_file, "X_SIZE\t= %u\n", x_size );
    fprintf( output_file, "Y_SIZE\t= %u\n", y_size );

    for ( int cbit = 0; cbit < color_bits; ++cbit )
    {
        static const char *card_name[] = { "MASTER", "SLAVE_1", "SLAVE_2", "SLAVE_3" };
        uint16_t cbit_offset = cbit * CARD_MEMORY_SIZE;

        fprintf( output_file, "\n\n%s:", card_name[cbit] );

        for ( int bytenum = 0; bytenum < data_size / color_bits; ++bytenum )
        {
            uint8_t databyte = data[cbit_offset + bytenum];

            if ( !( bytenum % BYTES_PER_LINE ) )
            {
                fprintf( output_file, "\n\t\t.BYTE\t$%2.2x", databyte );
            }
            else
            {
                fprintf( output_file, ", $%2.2x", databyte );
            }
        }
    }
        
    fclose( output_file );

    return true;
}

typedef uint16_t (*hex_write_fn)( FILE *output_file, uint16_t address, uint8_t *data, size_t data_size );
typedef bool (*hex_terminate_fn)( FILE *output_file, uint16_t lines );

bool output_hex( hex_write_fn write_fn, hex_terminate_fn terminate_fn, uint8_t *data, options_t *options, int data_size, int color_bits, uint16_t x_size, uint16_t y_size )
{
    FILE *output_file = fopen( options->output_filename, "w" );
    uint16_t lines = 0;

    if ( NULL == output_file )
    {
        perror( "Error opening output file" );
        return false;
    }

    for ( int cbit = 0; cbit < color_bits; ++cbit )
    {
        uint16_t cbit_offset = cbit * CARD_MEMORY_SIZE;
        uint16_t retlines;

        if ( x_size > ( MAX_COL_BYTES - 1 ) * 8 )
        {
            retlines = write_fn( output_file, options->base_address + cbit_offset, data + cbit_offset, data_size );
            if ( ! retlines )
            {
                fclose( output_file );
                return false;
            }
            lines += retlines;
        }
        else
        {
            for ( uint16_t linenum = 0; linenum < y_size; ++linenum )
            {
                uint16_t line_offset = linenum * MAX_COL_BYTES;
                retlines = write_fn( output_file, options->base_address + cbit_offset + line_offset, data + cbit_offset + linenum*((x_size+7)/8), (x_size+7)/8 );
                
                if ( ! retlines )
                {
                    fclose( output_file );
                    return false;
                }
                lines += retlines;
            }
        }
    }
    
    bool result = terminate_fn( output_file, lines );
    
    fclose( output_file );

    return result;
}

bool output_ihex( uint8_t *data, options_t *options, int data_size, int color_bits, uint16_t x_size, uint16_t y_size )
{
    return output_hex( ihex_write, ihex_terminate, data, options, data_size, color_bits, x_size, y_size );
}

bool output_pap( uint8_t *data, options_t *options, int data_size, int color_bits, uint16_t x_size, uint16_t y_size )
{
    return output_hex( pap_write, pap_terminate, data, options, data_size, color_bits, x_size, y_size );
}

void usage( char *myname )
{
    fprintf( stderr, "\nUsage: %s -i <input_file> [ -o <output_file> ] [-p <palette_file>] \\\n", basename( myname ) );
    fputs( "\t\t[ -f <format> ] [ -a <hex_base_addr> ]\n\n", stderr );

    fputs( "\tSupported formats:\n\n", stderr );
    for ( int f = 0; formats[f].format_string != NULL; ++f )
    {
        fprintf( stderr, "\t%s\t- %s\n", formats[f].format_string, formats[f].format_des );
    }
    fputs( "\n- If no output file is specified, same as input file with the appropriate\n", stderr );
    fputs( "  extension will be used.\n", stderr );
    fputs( "\n- If no palette file is specified, 1-bit black & white is assumed.\n", stderr );
    fprintf( stderr, "\n- Default base address is %4.4X. Min. is %4.4X, max. is %4.4X.\n", DEFAULT_BASE_ADDRESS, MIN_BASE_ADDRESS, MAX_BASE_ADDRESS );
}

bool get_options( int argc, char **argv, options_t *options )
{
    int c;
    char *cvalue = NULL;

    options->base_address = DEFAULT_BASE_ADDRESS;
    options->input_filename = NULL;
    options->output_filename = NULL;
    options->palette_filename = NULL;
    options->format = &formats[0];

    while (( c = getopt( argc, argv, "i:o:p:f:a:h?")) != -1 )
    {
        switch( c )
        {
            case 'i':
                options->input_filename = optarg;
                break;

            case 'o':
                options->output_filename = optarg;
                break;

            case 'p':
                options->palette_filename = optarg;
                break;
            
            case 'a':
                options->base_address = (uint16_t)strtoul( optarg, NULL, 16 );
                if ( errno )
                {
                    perror( "Invalid base address" );
                    return false;
                }
                if (    options->base_address < MIN_BASE_ADDRESS
                    ||  options->base_address > MAX_BASE_ADDRESS
                    ||  options->base_address % CARD_MEMORY_SIZE )
                {
                    fputs( "Invalid base address.\n", stderr );
                    return false;
                }

                break;
                        
            case 'f':
                int f;
                for ( f = 0; formats[f].format_string != NULL; ++f )
                {
                    if ( !strcmp( formats[f].format_string, optarg ) )
                    {
                        options->format = &formats[f];
                        break;
                    }
                }
                if ( NULL == formats[f].format_string )
                {
                    fprintf( stderr, "Unknown format: %s\n", optarg );
                    return false;
                }
                break;
            
            case 'h':
            case '?':
            default:
                return false;
        }

    }

    if ( NULL == options->input_filename )
    {
        fprintf( stderr, "Error: Missing input file.\n" );
        return false;
    }

    return true;
}

int main( int argc, char **argv )
{
    char read_buffer[BUFSIZ];
    uint8_t raw_image[MAX_IMAGE_SIZE];
    uint8_t converted_image[MAX_CARDS*CARD_MEMORY_SIZE];
    color_t color_palette[MAX_PALETTE_SIZE] = { { 0, 0, 0}, {255, 255, 255} };
    uint8_t color_translation[MAX_PALETTE_SIZE];

    options_t options;

    uint16_t x_size, y_size;

    int ncolors = 2, color_bits, image_size, data_size;

    FILE *image_file;
    
    if ( ! get_options( argc, argv, &options ) )
    {
        usage( argv[0] );
        exit( EXIT_FAILURE );
    }

    if ( NULL == options.output_filename )
    {
        options.output_filename = malloc( strlen( options.input_filename ) + strlen( options.format->format_string ) + 2 );
        if ( NULL == options.output_filename )
        {
            perror( "Error: Can't allocate output filename" );
            exit( EXIT_FAILURE );
        }

        strcpy( options.output_filename, options.input_filename );
        char *dot = strrchr ( options.output_filename, '.' );
        if ( dot == NULL )
        {
            strcat( options.output_filename, "." );
        }
        else
        {
            *++dot = '\0';
        }
        strcat( options.output_filename, options.format->format_string );

        printf( "Output file is '%s'\n", options.output_filename );
    }

    if ( NULL != options.palette_filename )
    {
        if ( 0 == ( ncolors = read_palette( options.palette_filename, read_buffer, sizeof( read_buffer ), color_palette ) ) )
        {
            exit( EXIT_FAILURE );
        }
    }
    else
    {
        puts( "Using default 1-bit black & white palette." );
    }
    
    if ( NULL == ( image_file = fopen( options.input_filename, "r" ) ) )
    {
        perror( "Error opening image file" );
        exit( EXIT_FAILURE );
    }

    if ( !get_image_dimensions( image_file, read_buffer, sizeof( read_buffer), &x_size, &y_size ) )
    {
        fputs( "Can't get image dimensions\n", stderr );
        exit( EXIT_FAILURE );
    }

    printf( "Image dimensions: %ux%u pixels\n", x_size, y_size );

    if ( x_size > MAX_COL_BYTES * 8 || y_size > MAX_ROWS )
    {
        fprintf( stderr, "Error: Max. image size is %ux%u\n", MAX_COL_BYTES * 8, MAX_ROWS );
        exit( EXIT_FAILURE );
    }

    if ( ncolors != translate_cmap( image_file, read_buffer, sizeof( read_buffer ), color_palette, color_translation, ncolors ) )
    {
        fputs( "Error: Palette does not match\n", stderr );
        exit( EXIT_FAILURE );
    }

    if ( !search_for_header_data( image_file, read_buffer, sizeof( read_buffer ) ) )
    {
        fputs( "Can't find image data\n", stderr );
        exit( EXIT_FAILURE );
    }

    if ( 0 == ( image_size = parse_image( image_file, read_buffer, sizeof( read_buffer ), raw_image, color_translation ) ) )
    {
        exit( EXIT_FAILURE );
    }

    fclose( image_file );
    
    printf( "Image size: %d pixels\n", image_size );

    if ( image_size != x_size * y_size )
    {
        fprintf( stderr, "Error: Expected image size is %d (Bad image file?)\n", x_size * y_size );
        exit( EXIT_FAILURE );
    }

    color_bits = (int)log2( ncolors );

    data_size = convert_to_layers( raw_image, converted_image, color_bits, x_size, y_size );
    
    options.format->output_fn( converted_image, &options, data_size, color_bits, x_size, y_size );

    exit( EXIT_SUCCESS );

}
