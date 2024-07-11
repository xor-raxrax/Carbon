using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Runtime.InteropServices;
using System.Text;

namespace CarbonGui
{
	class InjectApiResult
	{
		public InjectApiResult(string error)
		{
			this.error = error;
		}

		public static implicit operator bool(InjectApiResult what)
		{
			return !object.ReferenceEquals(what, null) && (what.error != null);
		}

		public string error;
	}

	static class InjectionApi
	{
		public static InjectApiResult SendScript(string data)
		{
			if (injectState != InjectState.ThreadGrabbed)
				return new InjectApiResult("not injected into thread");

            SendScript((ulong)data.Length, data);
            return new InjectApiResult(null);
		}

		public static InjectApiResult TryInject()
		{
			var pathsBytes = new List<byte>();

			// keep in sync with Inject()
			pathsBytes.AddRange(GetPathBytes(PathConfig.Dll));
			pathsBytes.AddRange(GetPathBytes(PathConfig.Settings));
			pathsBytes.AddRange(GetPathBytes(PathConfig.Dump));
			pathsBytes.AddRange(GetPathBytes(PathConfig.AddressDumper));
			pathsBytes.AddRange(GetPathBytes(PathConfig.UserDirectory));

			var size = (ulong)pathsBytes.Count;
			var arr = pathsBytes.ToArray();
			IntPtr result = Inject(size, arr);
			if (result != IntPtr.Zero)
				return new InjectApiResult(Marshal.PtrToStringAnsi(result));

			injectState = InjectState.Injected;
			return new InjectApiResult(null);
		}

		private static byte[] GetPathBytes(string path)
		{
			var absolutePath = Path.GetFullPath(path);
			var pathBytes = Encoding.Unicode.GetBytes(absolutePath);
			var lengthBytes = BitConverter.GetBytes((ulong)pathBytes.Length);
			return lengthBytes.Concat(pathBytes).ToArray();
		}

		public static InjectApiResult TryInjectEnvironment(string address)
		{
			if (IsValidHexAddress(address))
			{
				if (UInt64.TryParse(address, System.Globalization.NumberStyles.HexNumber, null, out ulong casted))
				{
					InjectEnvironment(casted);
					// TODO: should be received from client
					InjectionApi.injectState = InjectionApi.InjectState.ThreadGrabbed;
					return new InjectApiResult(null);
				}
			}

			return new InjectApiResult("passed address is not a valid hexadecimal address");
		}

		private static bool IsValidHexAddress(string address)
		{
			if (string.IsNullOrEmpty(address))
				return false;

			foreach (char c in address)
				if (!Uri.IsHexDigit(c))
					return false;

			return true;
		}
		public enum InjectState
		{
			None,
			Injected,
			ThreadGrabbed,
		}

		public static InjectState injectState = InjectState.None;

		[DllImport("Injector.dll", CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Unicode)]
		private static extern IntPtr Inject(ulong size, byte[] paths);

		[DllImport("Injector.dll", CallingConvention = CallingConvention.Cdecl)]
		private static extern void SendScript(ulong size, [MarshalAs(UnmanagedType.LPStr)] string data);

		[DllImport("Injector.dll", CallingConvention = CallingConvention.Cdecl)]
		private static extern void InjectEnvironment(ulong stateAddress);
	}
}
