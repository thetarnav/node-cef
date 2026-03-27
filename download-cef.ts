import * as fs   from "node:fs"
import * as path from "node:path"
import * as os   from "node:os"
import * as util from "node:util"
import * as bun  from "bun"

const CEF_VERSION      = "146.0.6+g68649e2"
const CHROMIUM_VERSION = "146.0.7680.154"

console.log(`[download-cef] Starting download script`)
console.log(`[download-cef] Node.js version: ${process.version}`)
console.log(`[download-cef] Platform: ${process.platform} ${process.arch}`)

let args = util.parseArgs({
	allow_positionals: true,
	strict: false,
	options: {
		platform: {type: "string"},
		force:    {type: "boolean"},
	},
}).values

let platform = process.platform
let arch     = process.arch

let cef_platform: string

switch (platform) {
	case "linux":  cef_platform = "linux64"                                       ;break
	case "darwin": cef_platform = arch === "arm64" ? "macosarm64" : "macosx64"   ;break
	case "win32":  cef_platform = arch === "arm64" ? "windowsarm64" : "windows64" ;break
	default:
		throw new Error(`Unsupported platform: ${platform}-${arch}`)
	}

if (args.platform) {
	cef_platform = String(args.platform)
	console.log(`[download-cef] Overriding platform to: ${cef_platform}`)
}

console.log(`[download-cef] Target CEF platform: ${cef_platform}`)
console.log(`[download-cef] Output directory: ${path.join(import.meta.dir, "cef", cef_platform)}`)

let output_dir = path.join(import.meta.dir, "cef", cef_platform)
let wrapper_lib_path: string

if (cef_platform.startsWith("windows")) {
	wrapper_lib_path = path.join(output_dir, "build", "libcef_dll_wrapper", "Release", "libcef_dll_wrapper.lib")
} else if (cef_platform.startsWith("macos")) {
	wrapper_lib_path = path.join(output_dir, "build", "libcef_dll_wrapper", "libcef_dll_wrapper.a")
} else {
	wrapper_lib_path = path.join(output_dir, "build", "libcef_dll_wrapper", "libcef_dll_wrapper.a")
}

let url = `https://cef-builds.spotifycdn.com/cef_binary_${CEF_VERSION}%2Bchromium-${CHROMIUM_VERSION}_${cef_platform}_minimal.tar.bz2`

console.log(`[download-cef] CEF URL: ${url}`)
console.log(`[download-cef] Expected wrapper: ${wrapper_lib_path}`)

let need_download = args.force || !fs.existsSync(output_dir)
if (!need_download) {
	let version_file = path.join(output_dir, ".version")
	if (fs.existsSync(version_file)) {
		let existing_version = fs.readFileSync(version_file, "utf-8")
		console.log(`[download-cef] CEF already exists at ${output_dir} (version: ${existing_version})`)
		if (fs.existsSync(wrapper_lib_path)) {
			console.log(`[download-cef] Wrapper library found, skipping download`)
			process.exit(0)
		} else {
			console.log(`[download-cef] Wrapper library missing, will rebuild`)
		}
	} else {
		console.log(`[download-cef] Version file missing, will re-download`)
	}
} else {
	console.log(`[download-cef] Force flag set or output dir missing, will download`)
}

console.log(`[download-cef] Creating output directory: ${output_dir}`)
fs.mkdirSync(output_dir, {recursive: true})

console.log(`[download-cef] Downloading CEF ${CEF_VERSION}+chromium-${CHROMIUM_VERSION}...`)
console.log(`[download-cef] From: ${url}`)

let tmp_dir = fs.mkdtempSync(path.join(os.tmpdir(), "node-cef-"))
console.log(`[download-cef] Temp directory: ${tmp_dir}`)
let archive = path.join(tmp_dir, `cef-${cef_platform}.tar.bz2`)

try {
	console.log(`[download-cef] Downloading with bun (fetch)...`)
	let response = await fetch(url)
	if (!response.ok) {
		throw new Error(`HTTP error! status: ${response.status}`)
	}
	let data = await response.arrayBuffer()
	fs.writeFileSync(archive, Buffer.from(data))

	let archive_size = fs.statSync(archive).size
	console.log(`[download-cef] Downloaded ${archive_size} bytes`)

	console.log(`[download-cef] Extracting archive...`)
	await bun.$`tar -xjf ${archive} --strip-components=1 -C ${output_dir}`

	console.log(`[download-cef] Writing version file`)
	fs.writeFileSync(path.join(output_dir, ".version"), `${CEF_VERSION}+chromium-${CHROMIUM_VERSION}`)

	console.log(`[download-cef] CEF extracted to ${output_dir}`)
	console.log(`[download-cef] Building CEF wrapper library...`)

	console.log(`[download-cef] Creating build directory`)
	fs.mkdirSync(path.join(output_dir, "build"), {recursive: true})

	let cmake_gen = "Unix Makefiles"
	if (platform === "win32") {
		cmake_gen = "Visual Studio 17 2022"
	} else if (platform === "darwin") {
		cmake_gen = "Xcode"
	}
	console.log(`[download-cef] Using CMake generator: ${cmake_gen}`)

	console.log(`[download-cef] Running CMake configure...`)
	await bun.$`cmake -G ${cmake_gen} -DCMAKE_BUILD_TYPE=Release -B build -S .`.cwd(output_dir)

	console.log(`[download-cef] Running CMake build for libcef_dll_wrapper...`)
	await bun.$`cmake --build build --target libcef_dll_wrapper -j ${os.cpus().length} --config Release`.cwd(output_dir)

	console.log(`[download-cef] Wrapper library built successfully`)
	console.log(`[download-cef] DONE`)
} catch (e) {
	console.error(`[download-cef] ERROR: ${e}`)
	throw e
} finally {
	console.log(`[download-cef] Cleaning up temp directory`)
    fs.rmSync(tmp_dir, {force: true, recursive: true})
}
