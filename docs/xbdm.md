xbdm_context_primer:
  name: "Xbox Debug Monitor / XBDM RDCP command research skill"
  purpose: >
    Help an LLM understand, use, document, and safely probe Xbox Debug Monitor
    commands, especially undocumented or sparsely documented commands from
    xboxdevwiki. Preserve exact command/response strings and avoid inventing
    unverified syntax.

  protocol:
    service: "Xbox Debug Monitor / XBDM"
    protocol_name: "RDCP"
    tcp_port: 731
    udp_port: 731
    transport: "TCP for command channel; UDP for name discovery"
    command_format: "ASCII text command followed by CRLF or CR-NUL"
    parameter_format: "Usually key=value; quote strings when needed"
    response_format: "NNN- message"
    multiline_response:
      code: 202
      terminator: "single line containing only '.'"
    binary_download_response:
      code: 203
      meaning: "server sends binary payload"
    binary_upload_response:
      code: 204
      meaning: "client must send binary payload"
    common_status_codes:
      200: "OK / command successful"
      201: "connected greeting"
      202: "multiline text follows"
      203: "binary response follows"
      204: "send binary data"
      400: "bad or missing parameter / generic failure"
      402: "file not found"
      404: "memory not mapped"
      405: "no such thread"
      407: "unknown command"
      408: "not stopped"
      409: "file must be copied"
      410: "file already exists"
      412: "invalid filename"
      414: "access denied"
      420: "box not locked"
      421: "key exchange required"
      422: "dedicated connection required"

  data_types:
    DWORD: "32-bit hex value, usually 0x12345678"
    QWORD: "64-bit hex value; XBDM often represents this as 0q0123456789ABCDEF"
    STRING: "ASCII string; quote values with spaces or ambiguous backslashes"
    HEXSTRING: "inline byte string such as 90909090"

  safety_rules:
    never_probe_without_snapshot_or_permission:
      - flash
      - fmtfat
      - sysfileupd
      - setsystime
      - setmem
      - delete
      - rename
      - sendfile
      - writefile
      - reboot
      - magicboot
    safe_initial_probe_commands:
      - dmversion
      - drivelist
      - 'dirlist name="E:\"'
      - 'dirlist name="C:\"'
      - 'dirlist name="D:\"'
      - title
      - systime
      - modules
      - threads
      - isstopped
      - isdebugger
      - xbeinfo
      - 'xbeinfo name="E:\default.xbe"'
    probing_method:
      - "Send bare command first."
      - "If 400/missing parameter, retry with likely required params: name=, thread=, addr=, length=."
      - "If 408/not stopped, stop or halt target before retrying debugger/context commands."
      - "If 422/dedicated required, open a fresh connection, send dedicate, then retry."
      - "If 414/access denied, try another drive/path or note privilege/security limitation."
      - "Record exact request line, exact response line, status code, and payload shape."
      - "Record whether response is single-line, multiline, or binary."
      - "Record environment: xemu, devkit, debug kit, retail+mod, XDK-launched title."
      - "Record XBDM version from dmversion."
      - "Do not promote guessed syntax to fact without live response evidence."

  command_reference:
    session:
      bye:
        summary: "Close RDCP connection."
        syntax:
          - bye

      dedicate:
        summary: "Move connection to dedicated command handler; required by some threaded commands."
        syntax:
          - dedicate
        notes:
          - "Use when server replies 422 dedicated connection required."
          - "After dedication, some normal/global commands may not behave the same."

      dmversion:
        summary: "Return XBDM/debug monitor version."
        syntax:
          - dmversion

    identity_and_network:
      dbgname:
        summary: "Get or set Xbox debug name used by UDP discovery / Xbox Neighborhood."
        syntax_candidates:
          - dbgname
          - 'dbgname name=STRING'

      servname:
        summary: "Get or set service name used by XBDM/Neighborhood."
        syntax_candidates:
          - servname
          - 'servname name=STRING'

      altaddr:
        summary: "Return alternate/title IP address."
        syntax_candidates:
          - altaddr

      boxid:
        summary: "Return console or box identifier."
        syntax_candidates:
          - boxid

    filesystem:
      drivelist:
        summary: "Return accessible drive letters, e.g. CDEFXYZ."
        syntax:
          - drivelist

      dirlist:
        summary: "List files/directories under path."
        syntax:
          - 'dirlist name=STRING'
        examples:
          - 'dirlist name="E:\"'
          - 'dirlist name="E:\TDATA\"'
        response_shape: "202 multiline; entries usually include name plus size/timestamp/attribute fields."

      getfile:
        summary: "Download whole file or file range from Xbox."
        syntax:
          - 'getfile name=STRING'
          - 'getfile name=STRING offset=DWORD size=DWORD'
        response_shape: "binary payload; often prefixed by 32-bit little-endian byte count."

      sendfile:
        summary: "Upload file to Xbox."
        syntax_candidates:
          - 'sendfile name=STRING length=DWORD'
        flow:
          - "client sends command"
          - "server replies 204"
          - "client sends exactly length bytes"
          - "server replies 200 or error"

      writefile:
        summary: "Write to an existing file or file range; lower-level than sendfile."
        syntax_candidates:
          - 'writefile name=STRING offset=DWORD length=DWORD'

      delete:
        summary: "Delete file, or delete directory when dir flag is present."
        syntax:
          - 'delete name=STRING'
          - 'delete name=STRING dir'

      mkdir:
        summary: "Create directory."
        syntax:
          - 'mkdir name=STRING'

      rename:
        summary: "Rename or move file/directory."
        syntax_candidates:
          - 'rename name=STRING newname=STRING'
          - 'rename name=STRING newname=STRING move'

      getfileattributes:
        summary: "Return file attributes, size, timestamps, directory/readonly/hidden flags."
        syntax_candidates:
          - 'getfileattributes name=STRING'

      setfileattributes:
        summary: "Set file attributes and/or timestamps."
        syntax_candidates:
          - 'setfileattributes name=STRING <attribute fields>'

      drivefreespace:
        summary: "Return free/total space for a drive or volume."
        syntax_candidates:
          - 'drivefreespace name=STRING'
          - 'drivefreespace drive=STRING'

      fileeof:
        summary: "Set or query file EOF/truncate size."
        syntax_candidates:
          - 'fileeof name=STRING size=DWORD'

      fmtfat:
        summary: "Format FAT/FATX partition."
        danger: "destructive"
        syntax_candidates:
          - 'fmtfat name=STRING'

    memory:
      getmem:
        summary: "Read target virtual memory."
        syntax:
          - 'getmem addr=DWORD length=DWORD'
        response_shape: "memory data, format may be text/hex or binary depending on build."

      getmem2:
        summary: "Alternate/newer memory read path; likely binary/chunked variant of getmem."
        syntax_candidates:
          - 'getmem2 addr=DWORD length=DWORD'

      setmem:
        summary: "Write target virtual memory."
        syntax_candidates:
          - 'setmem addr=DWORD data=HEXSTRING'
          - 'setmem addr=DWORD length=DWORD'
        notes:
          - "May use 204 binary upload flow on some builds."
          - "Dangerous; can crash title or console."

      getsum:
        summary: "Return checksum for memory blocks."
        syntax:
          - 'getsum addr=DWORD length=DWORD blocksize=DWORD'
        constraints:
          - "addr, length, and blocksize should be multiples of 8."
          - "Bad values may crash XBDM."
        algorithm_note: "ReverseBitOrder(CRC32(address+blockoffset, blocksize) XOR 0xFFFFFFFF)."

      walkmem:
        summary: "Walk/query memory map, regions, pages, and protections."
        syntax_candidates:
          - walkmem

      memtrack:
        summary: "Memory tracking or leak diagnostics."
        syntax_candidates:
          - memtrack

      mmglobal:
        summary: "Query memory-manager global state."
        syntax_candidates:
          - mmglobal

      modlong:
        summary: "Modify 32-bit memory value; possibly arithmetic/atomic memory operation."
        syntax_candidates:
          - 'modlong addr=DWORD value=DWORD'

    debugger_threads:
      threads:
        summary: "List threads."
        syntax:
          - threads
        response_shape: "multiline thread list."

      threadinfo:
        summary: "Return information for one thread."
        syntax_candidates:
          - 'threadinfo thread=DWORD'

      getcontext:
        summary: "Get CPU register context for stopped thread."
        syntax_candidates:
          - 'getcontext thread=DWORD'
          - 'getcontext thread=DWORD control integer fp full'
        notes:
          - "Usually requires stopped thread; otherwise 408."

      getextcontext:
        summary: "Get extended CPU context, likely FPU/MMX/SSE/debug registers."
        syntax_candidates:
          - 'getextcontext thread=DWORD'

      setcontext:
        summary: "Set CPU register context for stopped thread."
        syntax_candidates:
          - 'setcontext thread=DWORD <register fields or context blob>'

      stop:
        summary: "Stop/break target execution."
        syntax_candidates:
          - stop

      go:
        summary: "Continue/resume target execution globally."
        syntax_candidates:
          - go

      halt:
        summary: "Halt/break execution globally or for one thread."
        syntax_candidates:
          - halt
          - 'halt thread=DWORD'

      continue:
        summary: "Continue a stopped thread."
        syntax_candidates:
          - 'continue thread=DWORD'

      suspend:
        summary: "Suspend a thread."
        syntax_candidates:
          - 'suspend thread=DWORD'

      resume:
        summary: "Resume a suspended thread."
        syntax_candidates:
          - 'resume thread=DWORD'

      break:
        summary: "Manage software/data breakpoints or force break into debugger."
        syntax_candidates:
          - "break now"
          - "break start"
          - "break clearall"
          - "break addr=DWORD"
          - "break clear addr=DWORD"
          - "break read=DWORD size=DWORD"
          - "break write=DWORD size=DWORD"
          - "break readwrite=DWORD size=DWORD"
          - "break execute=DWORD size=DWORD"
          - "break clear read=DWORD"
          - "break clear write=DWORD"
          - "break clear readwrite=DWORD"
          - "break clear execute=DWORD"

      isbreak:
        summary: "Query whether a breakpoint exists or whether execution is at a breakpoint."
        syntax_candidates:
          - 'isbreak addr=DWORD'
          - 'isbreak thread=DWORD'

      isstopped:
        summary: "Query whether target or thread is stopped."
        syntax_candidates:
          - isstopped
          - 'isstopped thread=DWORD'

      isdebugger:
        summary: "Query whether debugger is attached."
        syntax_candidates:
          - isdebugger

      debugger:
        summary: "Attach/detach/query debugger state."
        syntax_candidates:
          - debugger
          - "debugger connect"
          - "debugger disconnect"
          - "debugger attach"
          - "debugger detach"

      debugmode:
        summary: "Get/set debug execution mode."
        syntax_candidates:
          - debugmode
          - 'debugmode mode=DWORD'

      dbgoptions:
        summary: "Get/set debugger option flags."
        syntax_candidates:
          - dbgoptions
          - 'dbgoptions flags=DWORD'

      stopon:
        summary: "Enable stop-on-event masks."
        syntax_candidates:
          - 'stopon <event flags>'

      nostopon:
        summary: "Disable stop-on-event masks."
        syntax_candidates:
          - 'nostopon <event flags>'

      funccall:
        summary: "Call target function at address with supplied args/register context."
        syntax_candidates:
          - 'funccall addr=DWORD'
          - 'funccall addr=DWORD arg0=DWORD arg1=DWORD'

      kd:
        summary: "Kernel debugger-related command; likely enable/disable/query KD state."
        syntax_candidates:
          - kd
          - 'kd enable'
          - 'kd disable'

    modules_and_title:
      modules:
        summary: "List loaded modules/XBEs/DLLs."
        syntax:
          - modules
        response_shape: "multiline module list."

      modsections:
        summary: "List sections for a loaded module."
        syntax_candidates:
          - 'modsections name=STRING'
          - 'modsections module=STRING'

      pdbinfo:
        summary: "Return PDB/debug-symbol information for title or module."
        syntax_candidates:
          - pdbinfo
          - 'pdbinfo name=STRING'

      xbeinfo:
        summary: "Return info about running or specified XBE."
        syntax_candidates:
          - xbeinfo
          - 'xbeinfo name=STRING'
        notes:
          - "Some builds return 400 missing name for bare xbeinfo."

      xtlinfo:
        summary: "Return XTL/Xbox runtime/library info for title."
        syntax_candidates:
          - xtlinfo

      title:
        summary: "Query running title, title path, or title metadata."
        syntax_candidates:
          - title

      deftitle:
        summary: "Get/set default debug title path."
        syntax_candidates:
          - deftitle
          - 'deftitle name=STRING'

      magicboot:
        summary: "Boot/launch another XBE. Always use 'debug' flag or the console reboots to dashboard. PATH must NOT be quoted."
        syntax_candidates:
          - 'magicboot title=STRING debug'
        notes: |
          The 'debug' keyword is REQUIRED for title launch. Without it the Xbox
          reboots to dashboard instead of loading the XBE.
          The path value must NOT be quoted: title=E:\GAMES\foo\default.xbe
          (not title="E:\GAMES\foo\default.xbe"). Quotes cause the file lookup
          to fail, which also falls through to the dashboard.

      reboot:
        summary: "Reboot console or reboot into title."
        syntax_candidates:
          - reboot
          - "reboot cold"
          - "reboot warm"
          - 'reboot title=STRING'
          - "reboot wait"

      getpid:
        summary: "Get current process/title process identifier."
        syntax_candidates:
          - getpid

    graphics_and_d3d:
      screenshot:
        summary: "Capture framebuffer screenshot."
        syntax_candidates:
          - screenshot
          - 'screenshot name=STRING'
        response_shape: "binary image/framebuffer data or saved file, depending on variant."

      getd3dstate:
        summary: "Retrieve Direct3D state."
        syntax_candidates:
          - getd3dstate

      getsurf:
        summary: "Read/capture a Direct3D surface, texture, or render target."
        syntax_candidates:
          - getsurf
          - 'getsurf <surface params>'

      getgamma:
        summary: "Get current gamma table."
        syntax_candidates:
          - getgamma

      getpalette:
        summary: "Read palette/color lookup table."
        syntax_candidates:
          - getpalette

      d3dopcode:
        summary: "Direct3D opcode/debug helper."
        syntax_candidates:
          - d3dopcode

      gpucount:
        summary: "Query/toggle GPU counters."
        syntax_candidates:
          - gpucount
          - "gpucount enable"
          - "gpucount disable"

      pbsnap:
        summary: "Pixel-buffer or push-buffer snapshot."
        syntax_candidates:
          - pbsnap

      pssnap:
        summary: "Pixel-shader, push-buffer, or surface snapshot."
        syntax_candidates:
          - pssnap

      vssnap:
        summary: "Vertex-shader or video/surface snapshot."
        syntax_candidates:
          - vssnap

      capcontrol:
        summary: "Older capture-control command."
        syntax_candidates:
          - capcontrol

      capctrl:
        summary: "Newer capture-control command replacing capcontrol on later builds."
        syntax_candidates:
          - capctrl

    notifications:
      notify:
        summary: "Open/create async debug notification channel."
        syntax_candidates:
          - notify

      notifyat:
        summary: "Configure/manage notification address or delivery target."
        syntax_candidates:
          - notifyat
          - 'notifyat addr=DWORD'

    security:
      adminpw:
        summary: "Clear or set administrator password/token."
        syntax_candidates:
          - "adminpw none"
          - "adminpw passwd=QWORD"
        notes:
          - "passwd appears to be a 64-bit value/token, not plain text."

      authuser:
        summary: "Authenticate as named XBDM user."
        syntax_candidates:
          - 'authuser name=STRING passwd=QWORD'

      user:
        summary: "Create, update, or delete XBDM user account."
        syntax_candidates:
          - 'user name=STRING'
          - 'user name=STRING passwd=QWORD'
          - 'user name=STRING delete'

      userlist:
        summary: "List XBDM users."
        syntax_candidates:
          - userlist

      getuserpriv:
        summary: "Get privilege level for current or specified user."
        syntax_candidates:
          - getuserpriv
          - 'getuserpriv name=STRING'

      setuserpriv:
        summary: "Set privilege level for XBDM user."
        syntax_candidates:
          - 'setuserpriv name=STRING priv=DWORD'
          - 'setuserpriv name=STRING priv=STRING'

      keyxchg:
        summary: "Perform security key exchange when lock/security mode requires it."
        syntax_candidates:
          - keyxchg

      lockmode:
        summary: "Get/set XBDM lock or security mode."
        syntax_candidates:
          - lockmode
          - "lockmode enabled"
          - "lockmode disabled"

    system_config:
      systime:
        summary: "Get console system time."
        syntax_candidates:
          - systime

      setsystime:
        summary: "Set console system time."
        syntax_candidates:
          - 'setsystime clocklo=DWORD clockhi=DWORD'
          - 'setsystime time=QWORD'
        notes:
          - "Has special failure behavior on some builds."

      setconfig:
        summary: "Set XBDM configuration option, likely persisted in config store."
        syntax_candidates:
          - 'setconfig name=STRING value=STRING'

      getutildrvinfo:
        summary: "Return utility drive or utility partition information."
        syntax_candidates:
          - getutildrvinfo

      sysfileupd:
        summary: "Update system file."
        danger: "potentially destructive"
        syntax_candidates:
          - sysfileupd

      signcontent:
        summary: "Sign content/package data."
        syntax_candidates:
          - signcontent

    dvd:
      dvdblk:
        summary: "Read raw DVD block/sector."
        syntax_candidates:
          - 'dvdblk block=DWORD count=DWORD'

      dvdperf:
        summary: "DVD performance or throughput diagnostic."
        syntax_candidates:
          - dvdperf

    performance:
      pclist:
        summary: "List performance counters."
        syntax_candidates:
          - pclist

      querypc:
        summary: "Query performance counter."
        syntax_candidates:
          - 'querypc name=STRING'
          - 'querypc type=DWORD'

    firmware:
      flash:
        summary: "Flash BIOS/firmware image."
        danger: "highly destructive"
        syntax_candidates:
          - flash

    unknown_or_very_sparse:
      crashdump:
        summary: "Crash dump configuration/retrieval."
      irtsweep:
        summary: "Unknown diagnostic/test command."
      lop:
        summary: "Unknown late-added command."

  llm_behavior_rules:
    - "Use exact command names and preserve casing as found."
    - "Do not invent exact syntax for sparse commands; use syntax_candidates."
    - "Separate observed behavior from inferred behavior."
    - "When writing KB entries, include command, args, response code, response text, payload shape, required state, privilege, and XBDM version."
    - "When debugging a failure, reason from status code first."
    - "400 usually means missing/bad parameter; try likely key=value params."
    - "408 means target/thread probably must be stopped."
    - "422 means retry through a dedicated connection."
    - "414 may mean drive/path/security issue rather than command syntax issue."
    - "Xbox 360 dmserv behavior can guide guesses, but never treat it as proof for Original Xbox."
    - "Maintain separate notes for xemu, real debug kit, and retail+modchip behavior."
