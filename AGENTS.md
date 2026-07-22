# VintaNetGS Agent Guidance

- VintaNetGS is an Apple IIgs ORCA/C plain-C project.
- Devel_Ops and TEXTUIGS are external sibling repositories and must not be modified from this repository.
- Use `vintanetgs_workflow.c` as the single compiler source.
- Keep canonical implementation in `include/` and `src/`.
- Never include the TEXTUIGS test program.
- Keep UI, config, serial transport, network protocol, and remote-control layers separate.
- Complete one migration phase before beginning the next.
- Do not port DOS UART register code directly.
- Do not port Borland-specific code directly.
- Do not introduce DOS `far`, interrupt, TSR, `conio`, or BIOS APIs.
- Do not change VintaNet protocol behavior without first documenting the corresponding DOS implementation.
- Do not automatically implement INFO polling.
- Keep discovery announcements separate from INFO request/response behavior.
- Preserve machine-name addressing and fallback when communications work begins.
- Do not add remote-control code until serial and packet communications are proven.
- After code or project changes, run the Apple IIgs workflow through build, import, and launch unless the user explicitly says not to.
- Provide complete changed files, not fragments, when reporting code changes.
