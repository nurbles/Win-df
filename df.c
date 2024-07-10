#define STRICT
#pragma warning( disable : 4996 )
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>

// ----------------------------------------------------------------------------

#define QWORD ULARGE_INTEGER

QWORD qwFreeBytesAvailable;
QWORD qwTotalNumberOfBytes;
QWORD qwTotalNumberOfFreeBytes;

int   nBufferLength;
char *szDriveList;
char *cp;
char  chDriveType = ' ';

BOOL  bCheckAll             = FALSE;
BOOL  bCheckNetDrives       = FALSE;
BOOL  bCheckRemovableDrives = FALSE;
BOOL  bCheckJunctions       = FALSE;
BOOL  bCheckRamDrives       = FALSE;
BOOL  bCheckCdromDrives     = FALSE;
BOOL  bCheckSubstDrives     = FALSE;
BOOL  bCheckMountDrives     = FALSE;

int   nNetDriveCount       = 0;
int   nRemovableDriveCount = 0;
int   nRamDriveCount       = 0;
int   nCdromDriveCount     = 0;
int   nSubstDriveCount     = 0;
int   nMountDriveCount     = 0;

// ----------------------------------------------------------------------------

#include <setupapi.h>
#include <devguid.h>
#include <cfgmgr32.h>

int usb_hard_drives( void )
{
   DWORD           PropertyRegDataType;
   DWORD           RequiredSize;
   DWORD           PropertyBuffer;
   DWORD           MemberIndex = 0;
   SP_DEVINFO_DATA sp_devinfo_data;
   HDEVINFO        hdevinfo = SetupDiGetClassDevs( &GUID_DEVCLASS_DISKDRIVE,
                                                   NULL, NULL, DIGCF_PRESENT );

   if( hdevinfo == INVALID_HANDLE_VALUE )
   {
      return -1;
   }
   ZeroMemory( &sp_devinfo_data, sizeof( sp_devinfo_data ) );
   sp_devinfo_data.cbSize = sizeof( sp_devinfo_data );
   int c = 0;
   while( SetupDiEnumDeviceInfo( hdevinfo, MemberIndex, &sp_devinfo_data ) )
   {
      if( SetupDiGetDeviceRegistryProperty( hdevinfo, &sp_devinfo_data,
                                            SPDRP_CAPABILITIES, &PropertyRegDataType,
                                            (PBYTE)&PropertyBuffer, sizeof( PropertyBuffer ),
                                            &RequiredSize ) ) {
         if( CM_DEVCAP_REMOVABLE == (PropertyBuffer & CM_DEVCAP_REMOVABLE) )
         {  // do something here to identify the drive letter.
            c++;
         }
      }
      MemberIndex++;
   }
   SetupDiDestroyDeviceInfoList( hdevinfo );
   return c;
}

// ----------------------------------------------------------------------------
// Function Name:  GetUniversalName
//
// Parameters:     szUniv  - contains the UNC equivalent of szDrive
//                           upon completion
//
//                 szDrive - contains a drive based path
//
// Return value:   TRUE if successful, otherwise FALSE
//
// Comments:       This function assumes that szDrive contains a
//                 valid drive based path.
//
//                 For simplicity, this code assumes szUniv points
//                 to a buffer large enough to accomodate the UNC
//                 equivalent of szDrive.

BOOL GetUniversalName( char szUniv[], char szDrive[] )
{
   HANDLE       hEnum;
   DWORD        dwResult;
   const int    c_cEntries = 0xFFFFFFFF;
   DWORD        cbBuffer;
   NETRESOURCE *pNetResource;
   BOOL         fResult;
   int          i;

   // get the local drive letter
   char chLocal = (char)toupper( szDrive[0] );

   // cursory validation
   if( chLocal < 'A' || chLocal > 'Z' )
      return FALSE;

   if( szDrive[1] != ':' || szDrive[2] != '\\' )
      return FALSE;

   dwResult = WNetOpenEnum( RESOURCE_CONNECTED, RESOURCETYPE_DISK,
                            0, NULL, &hEnum );

   if( dwResult != NO_ERROR )
      return FALSE;

   // start with a reasonable buffer size
   cbBuffer = 50 * sizeof( NETRESOURCE );
   pNetResource = (NETRESOURCE *)malloc( cbBuffer );

   fResult = FALSE;

   while( TRUE )
   {
      DWORD dwSize   = cbBuffer,
            cEntries = c_cEntries;

      dwResult = WNetEnumResource( hEnum, &cEntries, pNetResource,
                                   &dwSize );

      if( dwResult == ERROR_MORE_DATA )
      {
         // the buffer was too small, enlarge
         cbBuffer = dwSize;
         pNetResource = (NETRESOURCE *)realloc( pNetResource, cbBuffer );
         continue;
      }

      if( dwResult != NO_ERROR )
         goto done;

      // search for the specified drive letter
      for( i = 0; i < (int)cEntries; i++ )
      {
         if( pNetResource[i].lpLocalName &&
             chLocal == toupper( pNetResource[i].lpLocalName[0] ) )
         {
            // match
            fResult = TRUE;

            // build a UNC name
            strcpy( szUniv, pNetResource[i].lpRemoteName );
            strcat( szUniv, szDrive + 2 );
            _strupr( szUniv );
            goto done;
         }
      }
   }

done:
   // cleanup
   WNetCloseEnum( hEnum );
   free( pNetResource );

   return fResult;

}

// ----------------------------------------------------------------------------

char *Dotify( QWORD qw )
{
   static char  sz[14];
   DWORD b, m, t, h;

   b   = (DWORD)(qw.QuadPart / 1000000000);
   qw.QuadPart %=              1000000000;
   m   = (DWORD)(qw.QuadPart / 1000000);
   qw.QuadPart %=                 1000000;
   t   = (DWORD)(qw.QuadPart / 1000);
   qw.QuadPart %=                    1000;
   h   = (DWORD)(qw.QuadPart);

   if( b )
   {
      wsprintf( sz, "%u.%03u.%03u.%03u", b, m, t, h );
   }
   else if( m )
   {
      wsprintf( sz, "%u.%03u.%03u", m, t, h );
   }
   else if( t )
   {
      wsprintf( sz, "%u.%03u", t, h );
   }
   else
   {
      wsprintf( sz, "%u", h );
   }

   return sz;
}

// ----------------------------------------------------------------------------

char SetUnit( QWORD *pqw )
{
   char unit = 'K';

   if( (*pqw).QuadPart > 999999999999999999UI64 )  // Petabyte
   {
      unit = 'E';
      (*pqw).QuadPart = (*pqw).QuadPart / 1000000000000000UI64;
   }
   else if( (*pqw).QuadPart > 999999999999999UI64 )  // Terabyte
   {
      unit = 'P';
      (*pqw).QuadPart = (*pqw).QuadPart / 1000000000000UI64;
   }
   else if( (*pqw).QuadPart > 999999999999UI64 )  // Gigabyte
   {
      unit = 'T';
      (*pqw).QuadPart = (*pqw).QuadPart / 1000000000UI64;
   }
   else if( (*pqw).QuadPart > 999999999UI64 )  // Megabyte
   {
      unit = 'G';
      (*pqw).QuadPart = (*pqw).QuadPart / 1000000UI64;
   }
   else if( (*pqw).QuadPart > 999999UI64 )  // Kilobyte
   {
      unit = 'M';
      (*pqw).QuadPart = (*pqw).QuadPart / 1000UI64;
   }

   return unit;
}

// -----------------------------------------------------------------------------

LPSTR GetSystemErrorText( LPSTR lpMsgBuf, size_t sMsgBufLen, DWORD dwErrorCode )
{
   char *cpErr;
   DWORD dwLen;

   if( sMsgBufLen )
   {
      dwLen = FormatMessage( FORMAT_MESSAGE_FROM_SYSTEM,
                             NULL,
                             dwErrorCode,
                             MAKELANGID( LANG_ENGLISH, SUBLANG_ENGLISH_US ),
                             lpMsgBuf,
                             (DWORD)sMsgBufLen,
                             NULL );
      if( dwLen )
      {
         // Remove any trailing whitespace/cr/lf
         cpErr = lpMsgBuf + (dwLen - 1);
         while( dwLen && isspace( *cpErr ) )
         {
            *cpErr = '\0';
            cpErr--;
            dwLen--;
         }
      }
      else
      {
         char  szTemp[256];

         sprintf( szTemp, "%#8.8x (%u):  Unknown system error code",
                  dwErrorCode, dwErrorCode );
         strncpy( lpMsgBuf, szTemp, sMsgBufLen - 1 );
      }
      return lpMsgBuf;
   }
   return NULL;
}

// ----------------------------------------------------------------------------

BOOL CheckDrive( char *szDrive, BOOL bShare )
{
   char  tunits, uunits, funits;
   QWORD qwUsedBytes;
   char  szShare[_MAX_PATH];
   char *cpShare;
   DWORD percent;

   if( !GetDiskFreeSpaceEx( szDrive,
                            &qwFreeBytesAvailable,
                            &qwTotalNumberOfBytes,
                            &qwTotalNumberOfFreeBytes ) )
   {
      if( bCheckAll )
      {
         printf( "\r%.2s  %s                      \n", szDrive,
                 GetSystemErrorText( szShare, sizeof( szShare ), GetLastError() ) );
      }
      else
      {
         printf( "\r                               " );
      }
      return FALSE;
   }

   cpShare = szShare;
   if( bShare )
   {
      GetUniversalName( szShare, szDrive );
   }
   else
   {
      szDrive[2] = '\0';   // Remove trailing backslash
      if( 0 == QueryDosDevice( szDrive, szShare, sizeof( szShare ) ) )
      {  // Physical drive, put back root backslash
         sprintf( szShare, "%s\\", szDrive );
      }
      else
      {
         if( strncmp( "\\??\\", szShare, 4 ) )
         {  // Physical drive, put back root backslash
            sprintf( szShare, "%s\\", szDrive );
         }
         else
         {  // SUBST'd drive, skip leading goop
            if( bCheckSubstDrives )
            {
               cpShare = szShare + 4;
               nSubstDriveCount++;
            }
            else
            {
               szDrive[2] = '\\';   // Restore trailing backslash
               printf( "\r                               " );
               return FALSE;
            }
         }
      }
      szDrive[2] = '\\';   // Restore trailing backslash
   }

   qwUsedBytes.QuadPart = qwTotalNumberOfBytes.QuadPart
      - qwFreeBytesAvailable.QuadPart;

   percent = (DWORD)((qwUsedBytes.QuadPart * 100ui64)
                      / qwTotalNumberOfBytes.QuadPart);

   tunits = SetUnit( &qwTotalNumberOfBytes );
   uunits = SetUnit( &qwUsedBytes );
   funits = SetUnit( &qwFreeBytesAvailable );

   printf( "\r%2.2s   %c ", szDrive, chDriveType );
   printf( "%9.9s %c ", Dotify( qwTotalNumberOfBytes ), tunits );
   printf( "%9.9s %c ", Dotify( qwUsedBytes ), uunits );
   printf( "%9.9s %c ", Dotify( qwFreeBytesAvailable ), funits );
   printf( " %3u%%  %s\n", percent, cpShare );
   return TRUE;
}

// ----------------------------------------------------------------------------

void help( void )
{
   printf( "\n"
           "DF - Disk Free - v5.1\n"
           "     Copyright (C) 1996-2022, by Steve Valliere\n"
           "     All Rights Reserved\n"
           "\n"
           "     usage:  df [/A] [/c] [/j] [/m] [/n] [/p] [/r] [/s]"
           "\n"
           "             /A  check *ALL* drives\n"
           "             /c  check CDROM drives\n"
           "             /j  check mounted drives (Junctions)\n"
           "             /m  check RAM (memory) drives\n"
           "             /n  check network drives\n"
           "             /p  check MOUNTED drives (Admin shell REQUIRED)\n"
           "             /r  check removable drives\n"
           "             /s  check SUBST drives\n"
           "\n"
           "             /?  Show this help info\n"
   );
   exit( 1 );
}

// ----------------------------------------------------------------------------

int df_GetMountSpace( char *szVolume, char *szMountRoot )
{
   ULARGE_INTEGER FreeBytesAvailableToCaller;
   ULARGE_INTEGER TotalNumberOfBytes;
   ULARGE_INTEGER TotalNumberOfFreeBytes;
   ULARGE_INTEGER UsedBytes;
   char           tunits;
   char           uunits;
   char           funits;
   DWORD          percent;
   //char           szErr[256];

   if( GetDiskFreeSpaceExA( szVolume,
                            &FreeBytesAvailableToCaller,
                            &TotalNumberOfBytes,
                            &TotalNumberOfFreeBytes ) )
   {
      UsedBytes.QuadPart = TotalNumberOfBytes.QuadPart - TotalNumberOfFreeBytes.QuadPart;


      percent = (DWORD)((UsedBytes.QuadPart * 100ULL)
                         / TotalNumberOfBytes.QuadPart);

      tunits = SetUnit( &TotalNumberOfBytes );
      uunits = SetUnit( &UsedBytes );
      funits = SetUnit( &TotalNumberOfFreeBytes );

      printf( "\r%-3.3s  %c ", "<*>", 'P' );
      printf( "%9.9s %c ", Dotify( TotalNumberOfBytes ), tunits );
      printf( "%9.9s %c ", Dotify( UsedBytes ), uunits );
      printf( "%9.9s %c ", Dotify( TotalNumberOfFreeBytes ), funits );
      printf( " %3u%%  %s\n", percent, szMountRoot );
      nMountDriveCount++;
   }
   //else
   //{
   //   printf( "\r%.2s  %s\n", "MP",
   //           GetSystemErrorText( szErr, sizeof( szErr ), GetLastError() ) );
   //   printf( "    --> vol '%s'\n", szVolume );
   //   printf( "    -->  at '%s'\n", szMountRoot );
   //}
   return 0;
}

// ----------------------------------------------------------------------------

int df_GetMountName( char *szName, char *szVolume, char *szMount )
{
   char szMountPath[_MAX_PATH];

   sprintf( szMountPath, "%s%s", szVolume, szMount );
   if (!GetVolumeNameForVolumeMountPoint(szMountPath, szName, MAX_PATH))
   {
      *szName = '\0';
      //printf("GetVolumeNameForVolumeMountPoint failed.  Error = %d\n", GetLastError());
   }
   //else
   //{
   //   printf("  Mount VolName: %s\n", szName);
   //}
   return (int)strlen(szName);
}

// ----------------------------------------------------------------------------

void df_GetVolumeName( char *szDosDrive, char *szVolume )
{
   int    nStrLen;
   DWORD  dwBuffLen;
   LPTSTR szDrive;
   LPTSTR szBuffer = NULL;
   char   szVolumeName[MAX_PATH];

   // Get all logical drive strings
   dwBuffLen = GetLogicalDriveStrings(0, szBuffer);
   szBuffer = (LPTSTR)malloc(dwBuffLen*sizeof(char));
   GetLogicalDriveStrings(dwBuffLen, szBuffer);
   szDrive = szBuffer;

   //printf("Dos drive names: ");

   nStrLen = (int)strlen(szDrive);

   // Get the unique volume name for each logical drive string.  If the volume
   // drive string matches the passed in volume, print out the Dos drive name
   while(nStrLen)
   {
      if (GetVolumeNameForVolumeMountPoint(szDrive, szVolumeName, MAX_PATH))
      {
         if (_stricmp(szVolume, szVolumeName) == 0)
         {
            //printf(("%s "), szDrive);
            strcpy( szDosDrive, szDrive );
            break;
         }
      }
      szDrive += nStrLen+1;
      nStrLen = (int)strlen(szDrive);
   }

   //printf("\n");
   if (szBuffer) free(szBuffer);
   return;
}

// ----------------------------------------------------------------------------

void df_CheckMountPoints( char *szDosDrive, char *szVolume )
{
   HANDLE hFindMountPoint;
   char   szMountPoint[_MAX_PATH];
   char   szMountName[_MAX_PATH];
   char   szRootPath[_MAX_PATH];
   //char   szErr[256];
   //DWORD  dwErr;
   //UINT   driveType;

   // Find and print the first mount point.
   SetLastError( NO_ERROR );
   hFindMountPoint = FindFirstVolumeMountPoint( szVolume, szMountPoint, _MAX_PATH );

   // If a mount point was found, print it out, if there is not even
   // one mount point, just print "None" and return.
   if( hFindMountPoint != INVALID_HANDLE_VALUE )
   {
      if( df_GetMountName( szMountName, szVolume, szMountPoint ) )
      {
         strcpy( szRootPath, szDosDrive );
         strcat( szRootPath, szMountPoint );
         //driveType = GetDriveType( szRootPath );
         //printf(" %d    Mount Name: %s\n", driveType, szRootPath );
         //CheckDrive( szRootPath, FALSE );
         df_GetMountSpace( szMountName, szRootPath );
      }
   }
   else
   {
      //dwErr = GetLastError();
      //if( NO_ERROR != dwErr )
      //{
      //   printf( "FindFirstVolumeMountPoint failed for '%s'\n"
      //           "Error = %s\n", szVolume,
      //           GetSystemErrorText( szErr, sizeof( szErr ), GetLastError() ) );
      //}
      //printf("No mount points.\n" );
      return;
   }

   // Find and print the rest of the mount points
   while( FindNextVolumeMountPoint( hFindMountPoint, szMountPoint, _MAX_PATH ) )
   {
      if( df_GetMountName( szMountName, szVolume, szMountPoint ) )
      {
         strcpy( szRootPath, szDosDrive );
         strcat( szRootPath, szMountPoint );
         //driveType = GetDriveType( szRootPath );
         //printf(" %d    Mount Name: %s\n", driveType, szRootPath );
         //CheckDrive( szRootPath, FALSE );
         df_GetMountSpace( szMountName, szRootPath );
      }
   }

   FindVolumeMountPointClose( hFindMountPoint );
}

// ----------------------------------------------------------------------------

void df_CheckVolumes( void )
{
   HANDLE hFindVolume;
   char   szVolumeName[_MAX_PATH];
   char   szDosDrive[_MAX_PATH];
   char   szErr[256];
   // Find the first unique volume & enumerate it's mount points
   hFindVolume = FindFirstVolume( szVolumeName, _MAX_PATH );

   // If we can't even find one volume, just print an error and return.
   if( hFindVolume == INVALID_HANDLE_VALUE )
   {
      printf( "FindFirstVolume failed for '%s'\n"
              "Error = %s\n", szVolumeName,
              GetSystemErrorText( szErr, sizeof( szErr ), GetLastError() ) );
      return;
   }

   //printf( "\nUnique vol name: %s\n", szVolumeName );
   df_GetVolumeName( szDosDrive, szVolumeName );
   df_CheckMountPoints( szDosDrive, szVolumeName );

   // Find the rest of the unique volumes and enumerate each of their
   // mount points.
   while( FindNextVolume( hFindVolume, szVolumeName, MAX_PATH ) )
   {
      //printf( "\nUnique vol name: %s\n", szVolumeName );
      df_GetVolumeName( szDosDrive, szVolumeName );
      df_CheckMountPoints( szDosDrive, szVolumeName );
   }

   FindVolumeClose( hFindVolume );
   return;
}

// ----------------------------------------------------------------------------

void main( int argc, char **argv )
{
   int   a;
   UINT  driveType;

   while( --argc )
   {
      ++argv;
      if( ('/' == argv[0][0]) || ('-' == argv[0][0]) )
      {
         for( a=1; argv[0][a]; a++ )
         {
            switch( argv[0][a] )
            {
            case 'N':
               bCheckAll = TRUE;
            case 'n':
               bCheckNetDrives = TRUE;
               break;

            case 'R':
               bCheckAll = TRUE;
            case 'r':
               bCheckRemovableDrives = TRUE;
               break;

            case 'M':
               bCheckAll = TRUE;
            case 'm':
               bCheckRamDrives = TRUE;
               break;

            case 'C':
               bCheckAll = TRUE;
            case 'c':
               bCheckCdromDrives = TRUE;
               break;

            case 'J':
               bCheckAll = TRUE;
            case 'j':
               bCheckJunctions = TRUE;
               break;

            case 'P':
               bCheckAll = TRUE;
            case 'p':
               bCheckMountDrives = TRUE;
               break;

            case 'S':
               bCheckAll = TRUE;
            case 's':
               bCheckSubstDrives = TRUE;
               break;

            case 'A':
               bCheckAll             = TRUE;
               bCheckNetDrives       = TRUE;
               bCheckRemovableDrives = TRUE;
               bCheckJunctions       = TRUE;
               bCheckRamDrives       = TRUE;
               bCheckCdromDrives     = TRUE;
               bCheckSubstDrives     = TRUE;
               bCheckMountDrives     = TRUE;
               break;

            case '?':
               help();

            default:
               goto ShowUsage;
            }
         }
         continue;
      }
      else if( '?' == **argv )
      {
         help();
      }
ShowUsage:
      printf( "usage:  df [/A] [/c] [/j] [/m] [/n] [/p] [/r] [/s]" );
      exit( 1 );
   }

   nBufferLength = GetLogicalDriveStrings( 0, NULL );
   szDriveList = malloc( nBufferLength );
   if( szDriveList )
   {
      GetLogicalDriveStrings( nBufferLength, szDriveList );
      /*
              [----------- b y t e s -----------] <cr>
      drv typ     total        used        free    usage drv<cr>
      c:   F  9,999,999 K 9,999,999 K 9,999,999 K   999%  c:<cr>

      typ meaning
       C  CDROM drive
       J  Junction drive
       M  RAM drive
       N  Network drive
       P  Mounted drive
       R  Removable drive
       S  SUBST drive
       U  USB drive

      */
      printf( "\n" );
      printf( "        [----------- b y t e s -----------]        true\n" );
      printf( "drv typ     total        used        free    usage path\n" );

      cp = szDriveList;

      while( *cp )
      {
         printf( "\rChecking drive %s ...", cp );

         driveType = GetDriveType( cp );
         switch( driveType )
         {
         default:   // Error?
            chDriveType = 'x';
            printf( " (? %d ?)\r                               ", driveType );
            break;
         case DRIVE_UNKNOWN:
            chDriveType = '?';
            printf( " (unknown)\r                               " );
            break;
         case DRIVE_NO_ROOT_DIR:
            chDriveType = '-';
            printf( " (no root dir)\r                               " );
            break;
         case DRIVE_REMOVABLE:
            if( bCheckRemovableDrives )
            {
               chDriveType = 'R';
               if( CheckDrive( cp, FALSE ) ) nRemovableDriveCount++;
            }
            else
            {
               printf( "\r                               " );
            }
            break;
         case DRIVE_CDROM:
            if( bCheckCdromDrives )
            {
               chDriveType = 'C';
               if( CheckDrive( cp, FALSE ) ) nCdromDriveCount++;
            }
            else
            {
               printf( "\r                               " );
            }
            break;
         case DRIVE_REMOTE:
            if( bCheckNetDrives )
            {
               chDriveType = 'N';
               if( CheckDrive( cp, TRUE ) ) nNetDriveCount++;
            }
            else
            {
               printf( "\r                               " );
            }
            break;
         case DRIVE_RAMDISK:
            if( bCheckRamDrives )
            {
               chDriveType = 'M';
               if( CheckDrive( cp, FALSE ) ) nRamDriveCount++;
            }
            else
            {
               printf( "\r                               " );
            }
            break;
         case DRIVE_FIXED:
            chDriveType = 'F';
            CheckDrive( cp, FALSE );
            break;
         }
         cp += (lstrlen( cp ) + 1);
      }
      free( szDriveList );
      if( bCheckMountDrives )
      {
         chDriveType = 'P';
         df_CheckVolumes();
      }
      if( bCheckRemovableDrives && !nRemovableDriveCount )
      {
         printf( "\n    no removable drives detected" );
      }
      if( bCheckCdromDrives && !nCdromDriveCount )
      {
         printf( "\n    no CDROM drives detected" );
      }
      if( bCheckNetDrives && !nNetDriveCount )
      {
         printf( "\n    no network drives detected" );
      }
      if( bCheckRamDrives && !nRamDriveCount )
      {
         printf( "\n    no RAM drives detected" );
      }
      if( bCheckSubstDrives && !nSubstDriveCount )
      {
         printf( "\n    no SUBST drives detected" );
      }
      if( bCheckMountDrives && !nMountDriveCount )
      {
         printf( "\n    no MOUNTED drives detected" );
      }
   }
   exit( 0 );
}

// -----------------------------------------------------------------------

#if defined UsbExample
#include <windows.h>
#include <devguid.h>    // for GUID_DEVCLASS_CDROM etc
#include <setupapi.h>
#include <cfgmgr32.h>   // for MAX_DEVICE_ID_LEN, CM_Get_Parent and CM_Get_Device_ID
#define INITGUID
#include <tchar.h>
#include <stdio.h>

//#include "c:\WinDDK\7600.16385.1\inc\api\devpkey.h"

// include DEVPKEY_Device_BusReportedDeviceDesc from WinDDK\7600.16385.1\inc\api\devpropdef.h
#ifdef DEFINE_DEVPROPKEY
#undef DEFINE_DEVPROPKEY
#endif
#ifdef INITGUID
#define DEFINE_DEVPROPKEY(name, l, w1, w2, b1, b2, b3, b4, b5, b6, b7, b8, pid) EXTERN_C const DEVPROPKEY DECLSPEC_SELECTANY name = { { l, w1, w2, { b1, b2,  b3,  b4,  b5,  b6,  b7,  b8 } }, pid }
#else
#define DEFINE_DEVPROPKEY(name, l, w1, w2, b1, b2, b3, b4, b5, b6, b7, b8, pid) EXTERN_C const DEVPROPKEY name
#endif // INITGUID

// include DEVPKEY_Device_BusReportedDeviceDesc from WinDDK\7600.16385.1\inc\api\devpkey.h
DEFINE_DEVPROPKEY(DEVPKEY_Device_BusReportedDeviceDesc,  0x540b947e, 0x8b40, 0x45bc, 0xa8, 0xa2, 0x6a, 0x0b, 0x89, 0x4c, 0xbd, 0xa2, 4);     // DEVPROP_TYPE_STRING
DEFINE_DEVPROPKEY(DEVPKEY_Device_ContainerId,            0x8c7ed206, 0x3f8a, 0x4827, 0xb3, 0xab, 0xae, 0x9e, 0x1f, 0xae, 0xfc, 0x6c, 2);     // DEVPROP_TYPE_GUID
DEFINE_DEVPROPKEY(DEVPKEY_Device_FriendlyName,           0xa45c254e, 0xdf1c, 0x4efd, 0x80, 0x20, 0x67, 0xd1, 0x46, 0xa8, 0x50, 0xe0, 14);    // DEVPROP_TYPE_STRING
DEFINE_DEVPROPKEY(DEVPKEY_DeviceDisplay_Category,        0x78c34fc8, 0x104a, 0x4aca, 0x9e, 0xa4, 0x52, 0x4d, 0x52, 0x99, 0x6e, 0x57, 0x5a);  // DEVPROP_TYPE_STRING_LIST
DEFINE_DEVPROPKEY(DEVPKEY_Device_LocationInfo,           0xa45c254e, 0xdf1c, 0x4efd, 0x80, 0x20, 0x67, 0xd1, 0x46, 0xa8, 0x50, 0xe0, 15);    // DEVPROP_TYPE_STRING
DEFINE_DEVPROPKEY(DEVPKEY_Device_Manufacturer,           0xa45c254e, 0xdf1c, 0x4efd, 0x80, 0x20, 0x67, 0xd1, 0x46, 0xa8, 0x50, 0xe0, 13);    // DEVPROP_TYPE_STRING
DEFINE_DEVPROPKEY(DEVPKEY_Device_SecuritySDS,            0xa45c254e, 0xdf1c, 0x4efd, 0x80, 0x20, 0x67, 0xd1, 0x46, 0xa8, 0x50, 0xe0, 26);    // DEVPROP_TYPE_SECURITY_DESCRIPTOR_STRING

#define ARRAY_SIZE(arr)     (sizeof(arr)/sizeof(arr[0]))

#pragma comment (lib, "setupapi.lib")

typedef BOOL (WINAPI *FN_SetupDiGetDevicePropertyW)(
   __in       HDEVINFO DeviceInfoSet,
   __in       PSP_DEVINFO_DATA DeviceInfoData,
   __in       const DEVPROPKEY *PropertyKey,
   __out      DEVPROPTYPE *PropertyType,
   __out_opt  PBYTE PropertyBuffer,
   __in       DWORD PropertyBufferSize,
   __out_opt  PDWORD RequiredSize,
   __in       DWORD Flags
   );

// List all USB devices with some additional information
void ListDevices (CONST GUID *pClassGuid, LPCTSTR pszEnumerator)
{
   unsigned i, j;
   DWORD dwSize, dwPropertyRegDataType;
   DEVPROPTYPE ulPropertyType;
   CONFIGRET status;
   HDEVINFO hDevInfo;
   SP_DEVINFO_DATA DeviceInfoData;
   const static LPCTSTR arPrefix[3] = {TEXT("VID_"), TEXT("PID_"), TEXT("MI_")};
   TCHAR szDeviceInstanceID [MAX_DEVICE_ID_LEN];
   TCHAR szDesc[1024], szHardwareIDs[4096];
   WCHAR szBuffer[4096];
   LPTSTR pszToken, pszNextToken;
   TCHAR szVid[MAX_DEVICE_ID_LEN], szPid[MAX_DEVICE_ID_LEN], szMi[MAX_DEVICE_ID_LEN];
   FN_SetupDiGetDevicePropertyW fn_SetupDiGetDevicePropertyW = (FN_SetupDiGetDevicePropertyW)
      GetProcAddress (GetModuleHandle (TEXT("Setupapi.dll")), "SetupDiGetDevicePropertyW");

   // List all connected USB devices
   hDevInfo = SetupDiGetClassDevs (pClassGuid, pszEnumerator, NULL,
                                    pClassGuid != NULL ? DIGCF_PRESENT: DIGCF_ALLCLASSES | DIGCF_PRESENT);
   if (hDevInfo == INVALID_HANDLE_VALUE)
      return;

   // Find the ones that are driverless
   for (i = 0; ; i++)  {
      DeviceInfoData.cbSize = sizeof (DeviceInfoData);
      if (!SetupDiEnumDeviceInfo(hDevInfo, i, &DeviceInfoData))
         break;

      status = CM_Get_Device_ID(DeviceInfoData.DevInst, szDeviceInstanceID , MAX_PATH, 0);
      if (status != CR_SUCCESS)
         continue;

      // Display device instance ID
      _tprintf (TEXT("%s\n"), szDeviceInstanceID );

      if (SetupDiGetDeviceRegistryProperty (hDevInfo, &DeviceInfoData, SPDRP_DEVICEDESC,
                                             &dwPropertyRegDataType, (BYTE*)szDesc,
                                             sizeof(szDesc),   // The size, in bytes
                                             &dwSize))
         _tprintf (TEXT("    Device Description: \"%s\"\n"), szDesc);

      if (SetupDiGetDeviceRegistryProperty (hDevInfo, &DeviceInfoData, SPDRP_HARDWAREID,
                                             &dwPropertyRegDataType, (BYTE*)szHardwareIDs,
                                             sizeof(szHardwareIDs),    // The size, in bytes
                                             &dwSize)) {
         LPCTSTR pszId;
         _tprintf (TEXT("    Hardware IDs:\n"));
         for (pszId=szHardwareIDs;
               *pszId != TEXT('\0') && pszId + dwSize/sizeof(TCHAR) <= szHardwareIDs + ARRAYSIZE(szHardwareIDs);
               pszId += lstrlen(pszId)+1) {

            _tprintf (TEXT("        \"%s\"\n"), pszId);
         }
      }

      // Retreive the device description as reported by the device itself
      // On Vista and earlier, we can use only SPDRP_DEVICEDESC
      // On Windows 7, the information we want ("Bus reported device description") is
      // accessed through DEVPKEY_Device_BusReportedDeviceDesc
      if (fn_SetupDiGetDevicePropertyW && fn_SetupDiGetDevicePropertyW (hDevInfo, &DeviceInfoData, &DEVPKEY_Device_BusReportedDeviceDesc,
                                                                         &ulPropertyType, (BYTE*)szBuffer, sizeof(szBuffer), &dwSize, 0)) {

         if (fn_SetupDiGetDevicePropertyW (hDevInfo, &DeviceInfoData, &DEVPKEY_Device_BusReportedDeviceDesc,
                                            &ulPropertyType, (BYTE*)szBuffer, sizeof(szBuffer), &dwSize, 0))
            _tprintf (TEXT("    Bus Reported Device Description: \"%ls\"\n"), szBuffer);
         if (fn_SetupDiGetDevicePropertyW (hDevInfo, &DeviceInfoData, &DEVPKEY_Device_Manufacturer,
                                            &ulPropertyType, (BYTE*)szBuffer, sizeof(szBuffer), &dwSize, 0)) {
            _tprintf (TEXT("    Device Manufacturer: \"%ls\"\n"), szBuffer);
         }
         if (fn_SetupDiGetDevicePropertyW (hDevInfo, &DeviceInfoData, &DEVPKEY_Device_FriendlyName,
                                            &ulPropertyType, (BYTE*)szBuffer, sizeof(szBuffer), &dwSize, 0)) {
            _tprintf (TEXT("    Device Friendly Name: \"%ls\"\n"), szBuffer);
         }
         if (fn_SetupDiGetDevicePropertyW (hDevInfo, &DeviceInfoData, &DEVPKEY_Device_LocationInfo,
                                            &ulPropertyType, (BYTE*)szBuffer, sizeof(szBuffer), &dwSize, 0)) {
            _tprintf (TEXT("    Device Location Info: \"%ls\"\n"), szBuffer);
         }
         if (fn_SetupDiGetDevicePropertyW (hDevInfo, &DeviceInfoData, &DEVPKEY_Device_SecuritySDS,
                                            &ulPropertyType, (BYTE*)szBuffer, sizeof(szBuffer), &dwSize, 0)) {
            // See Security Descriptor Definition Language on MSDN
            // (http://msdn.microsoft.com/en-us/library/windows/desktop/aa379567(v=vs.85).aspx)
            _tprintf (TEXT("    Device Security Descriptor String: \"%ls\"\n"), szBuffer);
         }
         if (fn_SetupDiGetDevicePropertyW (hDevInfo, &DeviceInfoData, &DEVPKEY_Device_ContainerId,
                                            &ulPropertyType, (BYTE*)szDesc, sizeof(szDesc), &dwSize, 0)) {
            StringFromGUID2((REFGUID)szDesc, szBuffer, ARRAY_SIZE(szBuffer));
            _tprintf (TEXT("    ContainerId: \"%ls\"\n"), szBuffer);
         }
         if (fn_SetupDiGetDevicePropertyW (hDevInfo, &DeviceInfoData, &DEVPKEY_DeviceDisplay_Category,
                                            &ulPropertyType, (BYTE*)szBuffer, sizeof(szBuffer), &dwSize, 0))
            _tprintf (TEXT("    Device Display Category: \"%ls\"\n"), szBuffer);
      }

      pszToken = _tcstok_s (szDeviceInstanceID , TEXT("\\#&"), &pszNextToken);
      while(pszToken != NULL) {
         szVid[0] = TEXT('\0');
         szPid[0] = TEXT('\0');
         szMi[0] = TEXT('\0');
         for (j = 0; j < 3; j++) {
            if (_tcsncmp(pszToken, arPrefix[j], lstrlen(arPrefix[j])) == 0) {
               switch(j) {
               case 0:
                  _tcscpy_s(szVid, ARRAY_SIZE(szVid), pszToken);
                  break;
               case 1:
                  _tcscpy_s(szPid, ARRAY_SIZE(szPid), pszToken);
                  break;
               case 2:
                  _tcscpy_s(szMi, ARRAY_SIZE(szMi), pszToken);
                  break;
               default:
                  break;
               }
            }
         }
         if (szVid[0] != TEXT('\0'))
            _tprintf (TEXT("    vid: \"%s\"\n"), szVid);
         if (szPid[0] != TEXT('\0'))
            _tprintf (TEXT("    pid: \"%s\"\n"), szPid);
         if (szMi[0] != TEXT('\0'))
            _tprintf (TEXT("    mi: \"%s\"\n"), szMi);
         pszToken = _tcstok_s (NULL, TEXT("\\#&"), &pszNextToken);
      }
   }

   return;
}

int _tmain()
{
   // List all connected USB devices
   _tprintf (TEXT("---------------\n"));
   _tprintf (TEXT("- USB devices -\n"));
   _tprintf (TEXT("---------------\n"));
   ListDevices(NULL, TEXT("USB"));

   _tprintf (TEXT("\n"));
   _tprintf (TEXT("-------------------\n"));
   _tprintf (TEXT("- USBSTOR devices -\n"));
   _tprintf (TEXT("-------------------\n"));
   ListDevices(NULL, TEXT("USBSTOR"));

   _tprintf (TEXT("\n"));
   _tprintf (TEXT("--------------\n"));
   _tprintf (TEXT("- SD devices -\n"));
   _tprintf (TEXT("--------------\n"));
   ListDevices(NULL, TEXT("SD"));

   //_tprintf (TEXT("\n"));
   //ListDevices(&GUID_DEVCLASS_USB, NULL);
   //_tprintf (TEXT("\n"));

   _tprintf (TEXT("\n"));
   _tprintf (TEXT("-----------\n"));
   _tprintf (TEXT("- Volumes -\n"));
   _tprintf (TEXT("-----------\n"));
   //ListDevices(NULL, TEXT("STORAGE\\VOLUME"));
   //_tprintf (TEXT("\n"));
   ListDevices(&GUID_DEVCLASS_VOLUME, NULL);

   _tprintf (TEXT("\n"));
   _tprintf (TEXT("----------------------------\n"));
   _tprintf (TEXT("- devices with disk drives -\n"));
   _tprintf (TEXT("----------------------------\n"));
   ListDevices(&GUID_DEVCLASS_DISKDRIVE, NULL);

   return 0;
}
#endif
