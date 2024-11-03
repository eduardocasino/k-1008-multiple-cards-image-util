// Binary to iHEX format conversion routines.
//
// This utility converts GIMP images in C source code header format to a format
// suitable for display on a KIM-1 with one to four K-1008 cards as described in
// the "Use of the K-1008 for grey scale display, app note #2" document.
//
// (C) 2024 Eduardo Casino under the terms of the General Public License, Version 2
//
// https://github.com/eduardocasino/k-1008-multiple-cards-image-util
//
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

#ifndef UNUSED
#define UNUSED(x) (void)(x)
#endif

#define BYTES_PER_LINE 32

bool ihex_terminate( FILE *output_file, uint16_t lines )
{
    UNUSED( lines );

    if ( EOF == fputs( ":00000001FF\n", output_file ) )
    {
        perror( "Error writing to file" );
        return false;
    }

    return true;
}

uint16_t ihex_write( FILE *output_file, uint16_t address, uint8_t *data, size_t data_size )
{
    int byte_num = 0;
    uint8_t checksum = 0;
    uint16_t lines = 0;

    while ( byte_num < data_size )
    {
        if ( ! (byte_num % BYTES_PER_LINE) )
        {
            ++lines;

            // New line
            if ( byte_num )
            {
                // Print checksum
                if ( 0 > fprintf( output_file, "%2.2X\n", (uint8_t)(~checksum + 1 ) ) )
                {
                    perror( "Error writing to file" );
                    return 0;                    
                }
            }
            uint8_t bytes_in_line = data_size - byte_num > BYTES_PER_LINE ? BYTES_PER_LINE : data_size - byte_num;
            checksum = bytes_in_line + ( ( address >> 8 ) & 0xFF ) + ( address & 0xFF ) + 0x00;
            if ( 0 > fprintf( output_file, ":%2.2X%4.4X00", bytes_in_line, address ) )
            {
                perror( "Error writing to file" );
                return 0;                
            }
        }
        if ( 0 > fprintf( output_file, "%2.2X", data[byte_num] ) )
        {
            perror( "Error writing to file" );
            return 0;
        }

        checksum += data[byte_num];
        ++byte_num; ++address;
    }

    if ( 0 > fprintf( output_file, "%2.2X\n", (uint8_t)(~checksum + 1 ) ) )
    {
        perror( "Error writing to file" );
        lines = 0;
    }

    return lines;
}
