// This is a personal academic project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com

#include "pch.h"
#include "VisualDiskImager.h"
#include "VisualDiskImagerDlg.h"
#include "DeviceVolume.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

// CVisualDiskImagerDlg

void CVisualDiskImagerDlg::WriteDiskThread(CVisualDiskImagerDlg* pThis, LPCTSTR szFilename, LPCTSTR szDevice, bool bWrite, bool bVerifyAfterWrite)
{
	pThis->WriteDisk( szFilename, szDevice, bWrite, bVerifyAfterWrite );

	pThis->PostMessage( WM_DONE );
}

void CVisualDiskImagerDlg::WriteDisk(LPCTSTR szFilename, LPCTSTR szDevice, bool bWrite, bool bVerifyAfterWrite)
{
	Log( LOG_ACTION, IDS_FILE, szFilename );

	// Check file type
	WIN32_FILE_ATTRIBUTE_DATA wfad = {};
	if ( ! GetFileAttributesEx( szFilename, GetFileExInfoStandard, &wfad ) )
	{
		Log( LOG_ERROR, IDS_FILE_MISSING, (LPCTSTR)GetErrorString( GetLastError() ) );
		return;
	}
	if ( ( wfad.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY ) != 0 )
	{
		Log( LOG_ERROR, IDS_FILE_DIRECTORY );
		return;
	}

	// Open file
	CAtlFile file;
	HRESULT hr = file.Create( szFilename, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN );
	if ( FAILED( hr ) )
	{
		Log( LOG_ERROR, IDS_FILE_MISSING, (LPCTSTR)GetErrorString( hr ) );
		return;
	}

	// Check file size
	ULONGLONG file_size = 0;
	hr = file.GetSize( file_size );
	if ( FAILED( hr ) )
	{
		Log( LOG_ERROR, IDS_FILE_MISSING, (LPCTSTR)GetErrorString( hr ) );
		return;
	}
	if ( file_size == 0 )
	{
		Log( LOG_ERROR, IDS_FILE_ZERO_SIZE );
		return;
	}
	Log( LOG_INFO, IDS_FILE_SIZE, (LPCTSTR)FormatByteSizeEx( file_size ) );

	CDevice device( szDevice );
	device.GetDeviceVolumes( false );

	if ( bWrite )
	{
		// Lock device volumes
		for ( const auto& volume : device.Volumes )
		{
			volume->Lock();
		}

		// Unmount device volumes
		for ( const auto& volume : device.Volumes )
		{
			volume->Dismount();
		}
	}

	// Open disk
	Log( LOG_ACTION, IDS_DEVICE, szDevice );
	if ( ! device.Open( bWrite ) )
	{
		return;
	}

	// Check device size
	const ULONGLONG disk_size = device.Info.DiskSize.QuadPart;
	Log( LOG_INFO, IDS_DEVICE_SIZE, (LPCTSTR)FormatByteSizeEx( disk_size ) );
	if ( disk_size < file_size )
	{
		if ( disk_size == 0 )
		{
			Log( LOG_ERROR, IDS_DEVICE_SIZE_MISMATCH );
			return;
		}
		else
		{
			Log( LOG_WARNING, IDS_DEVICE_SIZE_MISMATCH );
		}
	}
	const DWORD bytes_per_sector = device.Info.Geometry.BytesPerSector ? device.Info.Geometry.BytesPerSector : 512;

	const DWORD buf_size = 4 * 1024 * bytes_per_sector;
	CVirtualBuffer< char > file_buf( buf_size );
	ULONGLONG pos = 0;
	const ULONGLONG size = min( file_size, disk_size );

	if ( bWrite && ! m_bCancel )
	{
		// Write to disk
		Log( LOG_ACTION, IDS_WRITING );

		while ( pos != size && ! m_bCancel )
		{
			const DWORD to_read = (DWORD)min( size - pos, buf_size );
			DWORD to_write = to_read;
			if ( to_read % bytes_per_sector != 0 )
			{
				to_write = ( to_read / bytes_per_sector + 1 ) * bytes_per_sector;

				// Read incomplete sector from device
				const DWORD last_pos = ( to_read / bytes_per_sector ) * bytes_per_sector;
				if ( FAILED( hr = device.Seek( pos, FILE_BEGIN ) ) ||
					 FAILED( hr = device.Read( static_cast< char* >( file_buf ) + last_pos, bytes_per_sector ) ) )
				{
					Log( LOG_ERROR, IDS_DEVICE_READ_ERROR, (LPCTSTR)GetErrorString( hr ) );
					break;
				}
			}

			if ( FAILED( hr = file.Seek( pos, FILE_BEGIN ) ) ||
				 FAILED( hr = file.Read( file_buf, to_read ) ) )
			{
				Log( LOG_ERROR, IDS_FILE_READ_ERROR, (LPCTSTR)GetErrorString( hr ) );
				break;
			}

			DWORD written = 0;
			if ( FAILED( hr = device.Seek( pos, FILE_BEGIN ) ) ||
				 FAILED( hr = device.Write( file_buf, to_write, &written ) ) ||
				 written != to_write )
			{
				pos += written;

				Log( LOG_ERROR, IDS_DEVICE_WRITE_ERROR, (LPCTSTR)GetErrorString( hr ) );
				break;
			}

			pos += to_read;

			m_nProgress = (int)( ( pos * 100 ) / file_size );
		}

		device.Flush();

		if ( pos == size )
		{
			// Success
			Log( LOG_INFO, IDS_WRITE_OK );
		}
		else
		{
			// Errors
			Log( LOG_WARNING, IDS_WRITE_ERROR, (LPCTSTR)FormatByteSize( pos ) );
		}
	}

	if ( ( ! bWrite || ( pos == size && bVerifyAfterWrite ) ) && ! m_bCancel )
	{
		// Verify disk
		Log( LOG_ACTION, IDS_VERIFYING );

		CVirtualBuffer< char > device_buf( buf_size );
		pos = 0;
		while ( pos != size && ! m_bCancel )
		{
			const DWORD to_read = (DWORD)min( size - pos, buf_size );
			DWORD to_verify = to_read;
			if ( to_read % bytes_per_sector != 0 )
			{
				to_verify = ( to_read / bytes_per_sector + 1 ) * bytes_per_sector;
			}

			if ( FAILED( hr = file.Seek( pos, FILE_BEGIN ) ) ||
				 FAILED( hr = file.Read( file_buf, to_read ) ) )
			{
				Log( LOG_ERROR, IDS_FILE_READ_ERROR, (LPCTSTR)GetErrorString( hr ) );
				break;
			}

			if ( FAILED( hr = device.Seek( pos, FILE_BEGIN ) ) ||
				 FAILED( hr = device.Read( device_buf, to_verify ) ) )
			{
				Log( LOG_ERROR, IDS_DEVICE_READ_ERROR, (LPCTSTR)GetErrorString( hr ) );
				break;
			}

			DWORD count = to_read;
			for ( char *p1 = file_buf, *p2 = device_buf; count; --count )
			{
				if ( *p1++ != *p2++ )
					break;
			}
			if ( count )
			{
				Log( LOG_ERROR, IDS_VERIFY_ERROR, pos + to_read - count );
				break;
			}

			pos += to_read;

			m_nProgress = (int)( ( pos * 100 ) / file_size );
		}

		if ( pos == size )
		{
			// Success
			Log( LOG_INFO, IDS_VERIFY_OK );
		}
	}

	if ( bWrite )
	{
		// Unlock device volumes
		for ( const auto& volume : device.Volumes )
		{
			volume->Unlock();
		}

		// Update device
		device.Update();
	}

	Log( LOG_INFO, IDS_DONE );
}
