import * as fs   from "node:fs"
import * as path from "node:path"
import * as cp   from "node:child_process"
import * as util from "node:util"

const CEF_VERSION      = "146.0.6+g68649e2"
const CHROMIUM_VERSION = "146.0.7680.154"

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
	case "darwin": cef_platform = arch === "arm64" ? "macosxarm64" : "macosx64"   ;break
	case "win32":  cef_platform = arch === "arm64" ? "windowsarm64" : "windows64" ;break
	default:
		throw new Error(`Unsupported platform: ${platform}-${arch}`)
	}

if (args.platform) {
	cef_platform = args.platform as string
}

let output_dir = path.join(import.meta.dir, "cef", cef_platform)
let url = `https://cef-builds.spotifycdn.com/cef_binary_${CEF_VERSION}%2Bchromium-${CHROMIUM_VERSION}_${cef_platform}_minimal.tar.bz2`

if (!args.force && fs.existsSync(output_dir)) {
	let version_file = path.join(output_dir, ".version")
	if (fs.existsSync(version_file)) {
		console.log(`CEF already exists at ${output_dir}, skipping download.`)
		process.exit(0)
	}
}

fs.mkdirSync(output_dir, {recursive: true})

console.log(`Downloading CEF ${CEF_VERSION}+chromium-${CHROMIUM_VERSION} for ${cef_platform}...`)
console.log(`From: ${url}`)

let temp_file = path.join(import.meta.dir, `cef-${cef_platform}.tar.bz2`)

try {
	cp.execSync(`curl -L -o "${temp_file}" "${url}"`, {stdio: "inherit"})

	console.log(`Download completed, extracting...`)

	cp.execSync(`tar -xjf "${temp_file}" --strip-components=1 -C "${output_dir}"`, {
		stdio: "inherit",
	})

	fs.writeFileSync(path.join(output_dir, ".version"), `${CEF_VERSION}+chromium-${CHROMIUM_VERSION}`)

	console.log(`CEF extracted to ${output_dir}`)
	console.log(`Building CEF wrapper library...`)

	fs.mkdirSync(path.join(output_dir, "build"), {recursive: true})
	cp.execSync(`cmake -G "Unix Makefiles" -DCMAKE_BUILD_TYPE=Release -B build -S .`, {
		cwd: output_dir,
		stdio: "inherit",
	})
	cp.execSync(`cmake --build build --target libcef_dll_wrapper -j8`, {
		cwd: output_dir,
		stdio: "inherit",
	})

	console.log(`CEF wrapper built successfully`)
} finally {
	if (fs.existsSync(temp_file)) {
		fs.unlinkSync(temp_file)
	}
}
