# Setting up the web flasher on GitHub

End result: a link like `https://yourname.github.io/frankenchan/` that flashes your StackChan and any sensor nodes straight from the browser. Anyone you share it with can use it too — no toolchain, no installs.

You do **not** need to know git. The whole thing is done in the browser.

---

## 1. Get a GitHub account

If you don't have one: <https://github.com/signup>. Free is fine.

## 2. Create the repository

1. Go to <https://github.com/new>
2. **Repository name:** `frankenchan`
3. **Visibility:** choose **Public**
   > GitHub Pages on a *private* repo needs a paid plan. Public is the easy path. Nothing secret lives in this repo — your Wi-Fi password and API keys get typed into the robot later, never into the code.
4. Leave "Add a README file" **unticked**
5. Click **Create repository**

## 3. Upload the firmware files

Unzip `frankenchan-firmware.zip` first. Inside you'll see `src`, `lib`, `flasher`, `extras`, `platformio.ini`, `README.md`, and a `.github` folder.

On the empty repo page, click the **uploading an existing file** link, then drag in **the contents** of the unzipped folder — not the folder itself.

> **The one thing that trips people up:** `.github` starts with a dot, so your file manager probably hides it. That folder contains the build instructions — without it, nothing happens.
> - **Windows:** File Explorer → View → tick **Hidden items**
> - **macOS:** press <kbd>Cmd</kbd>+<kbd>Shift</kbd>+<kbd>.</kbd> in Finder
>
> Then drag everything in, including `.github`.

Scroll down and click **Commit changes**.

**Verify:** your repo should now list `platformio.ini`, `src`, `flasher`, and `.github`. If `.github` is missing, do step 3a.

### 3a. If `.github` didn't upload

Create it by hand:

1. In your repo click **Add file → Create new file**
2. In the filename box type exactly: `.github/workflows/build.yml`
   (typing the slashes creates the folders for you)
3. Open `build.yml` from the unzipped package in Notepad/TextEdit, copy everything, paste it in
4. Click **Commit changes**

## 4. Turn on GitHub Pages

1. In your repo: **Settings** (top row) → **Pages** (left sidebar)
2. Under **Build and deployment → Source**, choose **GitHub Actions**

No branch to pick. Do this *before* worrying about build results — if you skip it, the build succeeds but publishing fails.

## 5. Run the build

Click the **Actions** tab.

- If a run is already going, just watch it.
- If it says workflows are disabled, click **I understand my workflows, go ahead and enable them**.
- To start one manually: **Build firmwares & deploy flasher** (left side) → **Run workflow** → **Run workflow**.

The first run takes **5–10 minutes** — it downloads the entire ESP32 toolchain. Later runs are much faster thanks to caching.

Green tick = done. Red X = see Troubleshooting.

## 6. Open your flasher

Go back to **Settings → Pages**. Your link is at the top:

```
https://yourname.github.io/frankenchan/
```

Open it in **Chrome or Edge on a desktop**. Safari and Firefox can't talk to USB devices, and phones can't either.

Pick your device, plug it in with USB-C, click **Install**.

## 7. Flash and set up

**StackChan:** after flashing it creates a Wi-Fi network `FrankenChan-xxxx` (password `stackchan`). Join it, the setup page opens, enter your Wi-Fi. From then on it lives at `http://frankenchan.local`, with a dashboard at `/dash`.

**Sensor node:** same idea — join `FC-Node-xxxx` (password `stackchan`), name it, set its direction.

---

## Troubleshooting

**Red X on the Actions run.** Click the failed run → click the failed step to expand the log. The last red lines say what broke. Send me that text and I'll fix it. Nothing is damaged — your robot is untouched until you actually flash something.

**"Get Pages site failed" or "Pages not enabled".** Step 4 was skipped or done after the run. Do step 4, then Actions → open the failed run → **Re-run all jobs**.

**Actions tab is empty.** `.github/workflows/build.yml` didn't upload. See step 3a.

**Flasher page loads but Install does nothing.** Wrong browser — use Chrome or Edge on a computer.

**No serial port appears.** Try another USB-C cable — a lot of them are charge-only. If it still doesn't show, hold the reset button while plugging in to force download mode.

**Page shows 404.** The build hasn't finished successfully yet. Check Actions for a green tick, then give it a minute.

---

## Changing things later

Edit any file directly on GitHub (click the file → pencil icon → **Commit changes**). Every commit rebuilds and republishes the flasher automatically — then reflash to pick up the changes.

Most day-to-day tweaks need no rebuild at all: personality, face style, colours, Wi-Fi, NFC tags, gestures and MQTT all live in the robot's own setup page.

## Going back to stock

Nothing here is permanent. Install [M5Burner](https://docs.m5stack.com/en/download), find StackChan, and flash the factory firmware to restore it.
