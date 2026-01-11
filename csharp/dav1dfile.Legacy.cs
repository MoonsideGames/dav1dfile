/* dav1dfile# - C# Wrapper for dav1dfile
 *
 * Copyright (c) 2023 Evan Hemsley
 *
 * This software is provided 'as-is', without any express or implied warranty.
 * In no event will the authors be held liable for any damages arising from
 * the use of this software.
 *
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute it
 * freely, subject to the following restrictions:
 *
 * 1. The origin of this software must not be misrepresented; you must not
 * claim that you wrote the original software. If you use this software in a
 * product, an acknowledgment in the product documentation would be
 * appreciated but is not required.
 *
 * 2. Altered source versions must be plainly marked as such, and must not be
 * misrepresented as being the original software.
 *
 * 3. This notice may not be removed or altered from any source distribution.
 *
 * Evan "cosmonaut" Hemsley <evan@moonside.games>
 *
 */

using System;
using System.Runtime.InteropServices;
using System.Text;

namespace Dav1dfile
{
	public static class Bindings
	{
		const string nativeLibName = "dav1dfile";

		public const uint DAV1DFILE_MAJOR_VERSION = 1;
		public const uint DAV1DFILE_MINOR_VERSION = 0;
		public const uint DAV1DFILE_PATCH_VERSION = 0;

		public const uint DAV1DFILE_COMPILED_VERSION = (
			(DAV1DFILE_MAJOR_VERSION * 100 * 100) +
			(DAV1DFILE_MINOR_VERSION * 100) +
			(DAV1DFILE_PATCH_VERSION)
		);

		[DllImport(nativeLibName, CallingConvention = CallingConvention.Cdecl)]
		public static extern uint df_linked_version();

		public enum PixelLayout
		{
			I400,
			I420,
			I422,
			I444
		}

		[DllImport(nativeLibName, CallingConvention = CallingConvention.Cdecl)]
		public extern static int df_open_from_memory(
			IntPtr bytes,
			uint size,
			out IntPtr context
		);

		[DllImport(nativeLibName, EntryPoint = "df_fopen", CallingConvention = CallingConvention.Cdecl)]
		private static extern unsafe int INTERNAL_df_fopen(
			[MarshalAs(UnmanagedType.LPStr)] string fname,
			out IntPtr context
		);

		[DllImport(nativeLibName, EntryPoint = "df_fopen", CallingConvention = CallingConvention.Cdecl)]
		private static extern unsafe int INTERNAL_df_fopen(
			byte* fname,
			out IntPtr context
		);

		[DllImport(nativeLibName, CallingConvention = CallingConvention.Cdecl)]
		public extern static void df_close(IntPtr context);

		[DllImport(nativeLibName, CallingConvention = CallingConvention.Cdecl)]
		public extern static void df_videoinfo(
			IntPtr context,
			out int width,
			out int height,
			out PixelLayout pixelLayout
		);

		[DllImport(nativeLibName, CallingConvention = CallingConvention.Cdecl)]
		public extern static void df_videoinfo2(
			IntPtr context,
			out int width,
			out int height,
			out PixelLayout pixelLayout,
			out byte hbd,
			out double fps
		);

		[DllImport(nativeLibName, CallingConvention = CallingConvention.Cdecl)]
		public extern static int df_eos(IntPtr context);

		[DllImport(nativeLibName, CallingConvention = CallingConvention.Cdecl)]
		public extern static void df_reset(IntPtr context);

		[DllImport(nativeLibName, CallingConvention = CallingConvention.Cdecl)]
		public extern static int df_readvideo(
			IntPtr context,
			int numFrames,
			out IntPtr yDataPtr,
			out IntPtr uDataPtr,
			out IntPtr vDataPtr,
			out uint yDataLength,
			out uint uvDataLength,
			out uint yStride,
			out uint uvStride
		);

		/* Used for heap allocated string marshaling
		 * Returned byte* must be free'd with FreeHGlobal.
		 */
		private static unsafe byte* Utf8Encode(string str)
		{
			int bufferSize = (str.Length * 4) + 1;
			byte* buffer = (byte*) Marshal.AllocHGlobal(bufferSize);
			fixed (char* strPtr = str)
			{
				Encoding.UTF8.GetBytes(
					strPtr,
					str.Length + 1,
					buffer,
					bufferSize
				);
			}

			return buffer;
		}

		public static unsafe int df_fopen(string fname, out IntPtr context)
		{
			int result;
			if (Environment.OSVersion.Platform == PlatformID.Win32NT)
			{
				/* Windows fopen doesn't like UTF8, use LPCSTR and pray */
				result = INTERNAL_df_fopen(fname, out context);
			}
			else
			{
				byte* utf8Fname = Utf8Encode(fname);
				result = INTERNAL_df_fopen(utf8Fname, out context);
				Marshal.FreeHGlobal((IntPtr) utf8Fname);
			}

			return result;
		}
	}
}
