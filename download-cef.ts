import { existsSync, mkdirSync, writeFileSync, unlinkSync } from "node:fs";
import { join } from "node:path";
import { execSync } from "node:child_process";
import { parseArgs } from "node:util";

const CEF_VERSION = "146.0.6+g68649e2";
const CHROMIUM_VERSION = "146.0.7680.154";

const args = parseArgs({
	allow_positionals: true,
	strict: false,
	options: {
		platform: { type: "string" },
		force: { type: "boolean" },
	},
}).values;

const platform = process.platform;
const arch = process.arch;

let cef_platform: string;

switch (platform) {
	case "linux":
		cef_platform = "linux64";
		break;
	case "darwin":
		cef_platform = arch === "arm64" ? "macosxarm64" : "macosx64";
		break;
	case "win32":
		cef_platform = arch === "arm64" ? "windowsarm64" : "windows64";
		break;
	default:
		throw new Error(`Unsupported platform: ${platform}-${arch}`);
	}

if (args.platform) {
	cef_platform = args.platform as string;
}

const output_dir = join(import.meta.dir, "cef", cef_platform);
const url = `https://cef-builds.spotifycdn.com/cef_binary_${CEF_VERSION}%2Bchromium-${CHROMIUM_VERSION}_${cef_platform}_minimal.tar.bz2`;

if (!args.force && existsSync(output_dir)) {
	const version_file = join(output_dir, ".version");
	if (existsSync(version_file)) {
		console.log(`CEF already exists at ${output_dir}, skipping download.`);
		process.exit(0);
	}
}

mkdirSync(output_dir, { recursive: true });

console.log(`Downloading CEF ${CEF_VERSION}+chromium-${CHROMIUM_VERSION} for ${cef_platform}...`);
console.log(`From: ${url}`);

const temp_file = join(import.meta.dir, `cef-${cef_platform}.tar.bz2`);

try {
	execSync(`curl -L -o "${temp_file}" "${url}"`, { stdio: "inherit" });

	console.log(`Download completed, extracting...`);

	execSync(`tar -xjf "${temp_file}" --strip-components=1 -C "${output_dir}"`, {
		stdio: "inherit",
	});

	writeFileSync(join(output_dir, ".version"), `${CEF_VERSION}+chromium-${CHROMIUM_VERSION}`);

	console.log(`CEF extracted to ${output_dir}`);
	console.log(`Building CEF wrapper library...`);

	mkdirSync(join(output_dir, "build"), { recursive: true });
	execSync(`cmake -G "Unix Makefiles" -DCMAKE_BUILD_TYPE=Release -B build -S .`, {
		cwd: output_dir,
		stdio: "inherit",
	});
	execSync(`cmake --build build --target libcef_dll_wrapper -j8`, {
		cwd: output_dir,
		stdio: "inherit",
	});

	console.log(`CEF wrapper built successfully`);
} finally {
	if (existsSync(temp_file)) {
		unlinkSync(temp_file);
	}
}