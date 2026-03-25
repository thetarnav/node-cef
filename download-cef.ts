import { existsSync, mkdirSync, writeFileSync, unlinkSync } from "fs";
import { join } from "path";
import { execSync } from "child_process";

const CEF_VERSION = "146.0.6+g68649e2";
const CHROMIUM_VERSION = "146.0.7680.154";
const PLATFORM = "linux64";
const OUTPUT_DIR = join(import.meta.dir, "cef", PLATFORM);

const URL = `https://cef-builds.spotifycdn.com/cef_binary_${CEF_VERSION}%2Bchromium-${CHROMIUM_VERSION}_${PLATFORM}_minimal.tar.bz2`;

async function downloadCEF() {
	if (existsSync(OUTPUT_DIR)) {
		const versionFile = join(OUTPUT_DIR, ".version");
		if (existsSync(versionFile)) {
			console.log(`CEF already exists at ${OUTPUT_DIR}, skipping download.`);
			return;
		}
	}

	mkdirSync(OUTPUT_DIR, { recursive: true });

	console.log(`Downloading CEF ${CEF_VERSION}+chromium-${CHROMIUM_VERSION} for ${PLATFORM}...`);
	console.log(`From: ${URL}`);

	const tempFile = join(import.meta.dir, `cef-${PLATFORM}.tar.bz2`);

	try {
		execSync(`curl -L -o "${tempFile}" "${URL}"`, { stdio: "inherit" });

		console.log(`Download completed, extracting...`);

		execSync(`tar -xjf "${tempFile}" --strip-components=1 -C "${OUTPUT_DIR}"`, {
			stdio: "inherit",
		});

		writeFileSync(join(OUTPUT_DIR, ".version"), `${CEF_VERSION}+chromium-${CHROMIUM_VERSION}`);

		console.log(`CEF extracted to ${OUTPUT_DIR}`);
		console.log(`Building CEF wrapper library...`);

		mkdirSync(join(OUTPUT_DIR, "build"), { recursive: true });
		execSync(`cmake -G "Unix Makefiles" -DCMAKE_BUILD_TYPE=Release -B build -S .`, {
			cwd: OUTPUT_DIR,
			stdio: "inherit",
		});
		execSync(`cmake --build build --target libcef_dll_wrapper -j8`, {
			cwd: OUTPUT_DIR,
			stdio: "inherit",
		});

		console.log(`CEF wrapper built successfully`);
	} finally {
		if (existsSync(tempFile)) {
			unlinkSync(tempFile);
		}
	}
}

downloadCEF().catch((error) => {
	console.error(`Failed to download CEF: ${error.message}`);
	process.exit(1);
});
