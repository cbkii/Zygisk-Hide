<h3 align="center">Zygisk Hide</h3>

  <p align="center">
    A Zygisk module that aims to hide the existence root and Zygisk, on a per-app basis.
    <br />
    <br />
    <a href="https://github.com/kgr-online/Zygisk-Hide/releases">Latest Release</a>
  </p>
</div>


<!-- ABOUT THE PROJECT -->
## About The Project

Using the **release** build is recommended over the debug build. Only use debug builds if you are going to make a bug report.

### KernelSU & APatch users:
1. Install ZygiskNext or NeoZygisk
1. Make sure the option `Umount modules/Exclude modifications` is enabled for the target app in the KernelSU/APatch Manager.
1. Disable `Enforce DenyList` in ZygiskNext settings if there is one.

### Magisk users:
1. Update your Magisk to 27.0 or newer for better hiding capabilities. (optional)
1. Turn on Zygisk in Magisk settings.
1. Turn off `Enforce DenyList` in Magisk settings.
1. Add the target app to the deny list unless you're using a Magisk fork with a white list instead.


<!-- FORK CHANGES -->
## Changes in this fork

This is a fork of the original [Zygisk Assistant](https://github.com/snake-4/Zygisk-Assistant) by [snake-4](https://github.com/snake-4), maintained by [kgr](https://github.com/kgr-online).

- **Added a module WebUI** (`webroot/`) for KernelSU / APatch managers, providing a per-app toggle list to enable or disable root/Zygisk hiding independently of the manager's built-in DenyList.
- Config is stored at `/data/adb/modules/zygisk-assistant/config.json`.

All credit for the original design, hiding logic, and upstream maintenance goes to snake-4 and the upstream contributors. This fork only adds the WebUI layer on top.


<!-- CONTRIBUTING -->
## Contributing

Contributions are what make the open source community such an amazing place to learn, inspire, and create. Any contributions you make are **greatly appreciated**.

If you have a suggestion that would make this better, please fork the repo and create a pull request. You can also simply open an issue with the tag "enhancement".
Don't forget to give the project a star! Thanks again!

1. Fork the Project
2. Create your Feature Branch (`git checkout -b feature/FeatureName`)
3. Commit your Changes (`git commit -m 'Add some FeatureName'`)
4. Push to the Branch (`git push origin feature/FeatureName`)
5. Open a Pull Request


<!-- CREDITS -->
## Credits

- [snake-4](https://github.com/snake-4) — original author and maintainer of Zygisk Assistant.
- [kgr](https://github.com/kgr-online) — WebUI fork.


<!-- LICENSE -->
## License

Distributed under the MIT License. See `LICENSE` for more information.
