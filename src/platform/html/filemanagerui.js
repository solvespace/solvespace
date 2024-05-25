"use strict";

const FileManagerUI_OPEN = 0;
const FileManagerUI_SAVE = FileManagerUI_OPEN + 1;
const FileManagerUI_BROWSE = FileManagerUI_SAVE + 1;

//FIXME(emscripten): File size thresholds. How large file can we accept safely ?

/** Maximum filesize for a uploaded file. 
 * @type {number} */
const FileManagerUI_UPLOAD_FILE_SIZE_LIMIT = 50 * 1000 * 1000;

const tryMakeDirectory = (path) => {
    try {
        FS.mkdir(path);
    } catch {
        // NOP
    }
}


class FileManagerUI {
    /**
     * @param {number} mode - dialog mode FileManagerUI_[ OPEN, SAVE, BROWSE ]
     */
    constructor(mode) {
        /** @type {boolean} */
        this.__isOpenDialog = false;
        /** @type {boolean} */
        this.__isSaveDialog = false;
        /** @type {boolean} */
        this.__isBrowseDialog = false;

        if (mode == FileManagerUI_OPEN) {
            this.__isOpenDialog = true;
        } else if (mode == FileManagerUI_SAVE) {
            this.__isSaveDialog = true;
        } else {
            this.__isBrowseDialog = true;
        }

        /** @type {boolean} true if the dialog is shown. */
        this.__isShown = false;

        /** @type {string[]}  */
        this.__extension_filters = [".slvs"];

        /** @type {string} */
        this.__basePathInFilesystem = "";

        /** @type {string} filename user selected. empty if nothing selected */
        this.__selectedFilename = "";

        this.__closedWithCancel = false;

        this.__defaultFilename = "untitled";
    }

    /** deconstructor
     */
    dispose() {
        if (this.__dialogRootElement) {
            this.__dialogHeaderElement = null;
            this.__descriptionElement = null;
            this.__filelistElement = null;
            this.__fileInputElement = null;
            this.__saveFilenameInputElement = null;
            this.__buttonContainerElement = null;
            this.__dialogRootElement.parentElement.removeChild(this.__dialogRootElement);
            this.__dialogRootElement = null;
        }
    }

    /** 
     * @param {string} label 
     * @param {string} response 
     * @param {bool} isDefault 
     */
    __addButton(label, response, isDefault, onclick) {
        const buttonElem = document.createElement("div");
        addClass(buttonElem, "button");
        setLabelWithMnemonic(buttonElem, label);
        if (isDefault) {
            addClass(buttonElem, "default");
            addClass(buttonElem, "selected");
        }
        buttonElem.addEventListener("click", () => {
            if (onclick) {
                if (onclick()) {
                    this.__close();
                }
            } else {
                this.__close();
            }
        });

        this.__buttonContainerElement.appendChild(buttonElem);
    }

    /**
     * @param {HTMLElement} div element that built
     */
    buildDialog() {
        const root = document.createElement('div');
        addClass(root, "modal");
        root.style.display = "none";
        root.style.zIndex = 1000;

        const dialog = document.createElement('div');
        addClass(dialog, "dialog");
        addClass(dialog, "wide");
        root.appendChild(dialog);

        const messageHeader = document.createElement('strong');
        this.__dialogHeaderElement = messageHeader;
        addClass(messageHeader, "dialog_header");
        dialog.appendChild(messageHeader);

        const description = document.createElement('p');
        this.__descriptionElement = description;
        dialog.appendChild(description);

        const filelistheader = document.createElement('h3');
        filelistheader.textContent = 'Files:';
        dialog.appendChild(filelistheader);

        const filelist = document.createElement('ul');
        this.__filelistElement = filelist;
        addClass(filelist, 'filelist');
        dialog.appendChild(filelist);

        const dummyfilelistitem = document.createElement('li');
        dummyfilelistitem.textContent = "(No file in pseudo filesystem)";
        filelist.appendChild(dummyfilelistitem);

        if (this.__isOpenDialog) {
            const fileuploadcontainer = document.createElement('div');
            dialog.appendChild(fileuploadcontainer);

            const fileuploadheader = document.createElement('h3');
            fileuploadheader.textContent = "Upload file:";
            fileuploadcontainer.appendChild(fileuploadheader);

            const dragdropdescription = document.createElement('p');
            dragdropdescription.textContent = "(Drag & drop file to the following box)";
            dragdropdescription.style.fontSize = "0.8em";
            dragdropdescription.style.margin = "0.1em";
            fileuploadcontainer.appendChild(dragdropdescription);

            const filedroparea = document.createElement('div');
            addClass(filedroparea, 'filedrop');
            filedroparea.addEventListener('dragstart', (ev) => this.__onFileDragDrop(ev));
            filedroparea.addEventListener('dragover', (ev) => this.__onFileDragDrop(ev));
            filedroparea.addEventListener('dragleave', (ev) => this.__onFileDragDrop(ev));
            filedroparea.addEventListener('drop', (ev) => this.__onFileDragDrop(ev));
            fileuploadcontainer.appendChild(filedroparea);

            const fileinput = document.createElement('input');
            this.__fileInputElement = fileinput;
            fileinput.setAttribute('type', 'file');
            fileinput.style.width = "100%";
            fileinput.addEventListener('change', (ev) => this.__onFileInputChanged(ev));
            filedroparea.appendChild(fileinput);
        
        } else if (this.__isSaveDialog) {
            const filenameinputcontainer = document.createElement('div');
            dialog.appendChild(filenameinputcontainer);

            const filenameinputheader = document.createElement('h3');
            filenameinputheader.textContent = "Filename:";
            filenameinputcontainer.appendChild(filenameinputheader);

            const filenameinput = document.createElement('input');
            filenameinput.setAttribute('type', 'input');
            filenameinput.style.width = "90%";
            filenameinput.style.margin = "auto 1em auto 1em";
            this.__saveFilenameInputElement = filenameinput;
            filenameinputcontainer.appendChild(filenameinput);
        }

        // Paragraph element for spacer
        dialog.appendChild(document.createElement('p'));

        const buttoncontainer = document.createElement('div');
        this.__buttonContainerElement = buttoncontainer;
        addClass(buttoncontainer, "buttons");
        dialog.appendChild(buttoncontainer);

        this.__addButton('OK', 0, false, () => {
            if (this.__isOpenDialog) {
                let selectedFilename = null;
                const fileitems = document.querySelectorAll('input[type="radio"][name="filemanager_filelist"]');
                Array.from(fileitems).forEach((radiobox) => {
                    if (radiobox.checked) {
                        selectedFilename = radiobox.parentElement.getAttribute('data-filename');
                    }
                });
                if (selectedFilename) {
                    return true;
                } else {
                    return false;
                }
            } else {
                return true;
            }
        });

        this.__addButton('Cancel', 1, true, () => {
            this.__closedWithCancel = true;
            return true;
        });

        return root;
    }

    /**
     * @param {string} text 
     */
    setTitle(text) {
        this.__dialogHeaderText = text;
    }

    /**
     * @param {string} text
     */
    setDescription(text) {
        this.__descriptionText = text;
    }

    /**
     * @param {string} path file prefix. (ex) 'tmp/' to '/tmp/filename.txt'
     */
    setBasePath(path) {
        this.__basePathInFilesystem = path;
        tryMakeDirectory(path);
    }

    /**
     * @param {string} filename 
     */
    setDefaultFilename(filename) {
        this.__defaultFilename = filename;
    }

    /**
     * 
     * @param {string} filter comma-separated extensions like ".slvs,.stl;."
     */
    setFilter(filter) {
        const exts = filter.split(',');
        this.__extension_filters = exts;
    }

    __buildFileEntry(filename) {
        const lielem = document.createElement('li');
        const label = document.createElement('label');
        label.setAttribute('data-filename', filename);
        lielem.appendChild(label);
        const radiobox = document.createElement('input');
        radiobox.setAttribute('type', 'radio');
        if (!this.__isOpenDialog) {
            radiobox.style.display = "none";
        }
        radiobox.setAttribute('name', 'filemanager_filelist');
        label.appendChild(radiobox);
        const filenametext = document.createTextNode(filename);
        label.appendChild(filenametext);

        return lielem;
    }

    /**
     * @returns {string[]} filename array
     */
    __getFileEntries() {
        const basePath = this.__basePathInFilesystem;
        /** @type {any[]} */
        const nodes = FS.readdir(basePath);
        /** @type {string[]} */
        const files = nodes.filter((nodename) => {
            return FS.isFile(FS.lstat(basePath + nodename).mode);
        });
        /*.map((filename) => {
            return basePath + filename;
        });*/
        console.log(`__getFileEntries():`, files);
        return files;
    }
    
    /**
     * @param {string[]?} files  file list already constructed
     * @returns {string[]} filename array
     */
     __getFileEntries_recurse(basePath) {
        //FIXME:remove try catch block
        try {
            //const basePath = this.__basePathInFilesystem;
            FS.currentPath = basePath;
            /** @type {any[]} */
            const nodes = FS.readdir(basePath);

            const filesInThisDirectory = nodes.filter((nodename) => {
                return FS.isFile(FS.lstat(basePath + "/" + nodename).mode);
            }).map((filename) => {
                return basePath + "/" + filename;
            });
            let files = filesInThisDirectory;

            const directories = nodes.filter((nodename) => {
                return FS.isDir(FS.lstat(basePath + "/" + nodename).mode);
            });

            for (let i = 0; i < directories.length; i++) {
                const directoryname = directories[i];
                if (directoryname == '.' || directoryname == '..') {
                    continue;
                }
                const orig_cwd = FS.currentPath;
                const directoryfullpath = basePath + "/" + directoryname;
                FS.currentPath = directoryfullpath;
                files = files.concat(this.__getFileEntries_recurse(directoryfullpath));
                FS.currentPath = orig_cwd;
            }
            
            console.log(`__getFileEntries_recurse(): in "${basePath}"`, files);
            return files;

        } catch (excep) {
            console.log(excep);
            throw excep;
        }
    }

    __updateFileList() {
        console.log(`__updateFileList()`);
        Array.from(this.__filelistElement.children).forEach((elem) => {
            this.__filelistElement.removeChild(elem);
        });
        // const files = this.__getFileEntries();
        FS.currentPath = this.__basePathInFilesystem;
        const files = this.__getFileEntries_recurse(this.__basePathInFilesystem);
        if (files.length < 1) {
            const dummyfilelistitem = document.createElement('li');
            dummyfilelistitem.textContent = "(No file in pseudo filesystem)";
            this.__filelistElement.appendChild(dummyfilelistitem);

        } else {
            files.forEach((entry) => {
                this.__filelistElement.appendChild(this.__buildFileEntry(entry));
            });
        }
    }


    /**
     * @param {File} file 
     */
    __getFileAsArrayBuffer(file) {
        return new Promise((resolve, reject) => {
            const filereader = new FileReader();
            filereader.onerror = (ev) => {
                reject(ev);
            };
            filereader.onload = (ev) => {
                resolve(ev.target.result);
            };
            filereader.readAsArrayBuffer(file);
        });
    }

    /**
     * 
     * @param {File} file 
     */
    async __tryAddFile(file) {
        return new Promise(async (resolve, reject) => {
            if (!file) {
                reject(new Error(`Invalid arg: file is ${file}`));

            } else if (file.size > FileManagerUI_UPLOAD_FILE_SIZE_LIMIT) {
                //FIXME(emscripten): Use our MessageDialog instead of browser's alert().
                alert(`Specified file is larger than limit of ${FileManagerUI_UPLOAD_FILE_SIZE_LIMIT} bytes. Canceced.`);
                reject(new Error(`File is too large: "${file.name} is ${file.size} bytes`));
                
            } else {
                // Just add to Filesystem
                const path = `${this.__basePathInFilesystem}${file.name}`;
                const blobArrayBuffer = await this.__getFileAsArrayBuffer(file);
                const u8array = new Uint8Array(blobArrayBuffer);
                const fs = FS.open(path, "w");
                FS.write(fs, u8array, 0, u8array.length, 0);
                FS.close(fs);
                resolve(); 
            }
        });
    }

    __addSelectedFile() {
        if (this.__fileInputElement.files.length < 1) {
            console.warn(`No file selected.`);
            return;
        }

        const file = this.__fileInputElement.files[0];
        this.__tryAddFile(file)
            .then(() => {
                this.__updateFileList();
            })
            .catch((err) => {
                this.__fileInputElement.value = null;
                console.error(err);
            })
    }

    /**
     * @param {DragEvent} ev 
     */
    __onFileDragDrop(ev) {
        ev.preventDefault();
        if (ev.type == "dragenter" || ev.type == "dragover" || ev.type == "dragleave") {
            return;
        }
        if (ev.dataTransfer.files.length < 1) {
            return;
        }
        this.__fileInputElement.files = ev.dataTransfer.files;

        this.__addSelectedFile();
    }

    /**
     * @param {InputEvent} _ev 
     */
    __onFileInputChanged(_ev) {
        this.__addSelectedFile();
    }


    /** Show the FileManager UI dialog */
    __show() {
        this.__closedWithCancel = false;

        /** @type {HTMLElement} */
        this.__dialogRootElement = this.buildDialog();
        document.querySelector('body').appendChild(this.__dialogRootElement);

        this.__dialogHeaderElement.textContent = this.__dialogHeaderText || "File manager";
        this.__descriptionElement.textContent = this.__descriptionText || "Select a file.";
        if (this.__extension_filters) {
            this.__descriptionElement.textContent += "Requested filter is " + this.__extension_filters.join(", ");
        }

        if (this.__isOpenDialog && this.__extension_filters) {
            this.__fileInputElement.accept = this.__extension_filters.concat(',');
        }

        if (this.__isSaveDialog) {
            this.__saveFilenameInputElement.value = this.__defaultFilename;
        }

        this.__dialogRootElement.style.display = "block";
        this.__isShown = true;
    }

    /** Close the dialog */
    __close() {
        this.__selectedFilename = "";
        if (this.__isOpenDialog) {
            Array.from(document.querySelectorAll('input[type="radio"][name="filemanager_filelist"]'))
                .forEach((elem) => {
                    if (elem.checked) {
                        this.__selectedFilename = elem.parentElement.getAttribute("data-filename");
                    }
                });
        } else if (this.__isSaveDialog) {
            if (!this.__closedWithCancel) {
                this.__selectedFilename = this.__saveFilenameInputElement.value;
            }
        }

        Array.from(this.__filelistElement.children).forEach((elem) => {
            this.__filelistElement.removeChild(elem);
        });

        this.dispose();

        this.__isShown = false;
    }

    /**
     * @return {boolean}
     */
    isShown() {
        return this.__isShown;
    }

    /**
     * 
     * @returns {Promise} filename string on resolved.
     */
    showModalAsync() {
        return new Promise((resolve, reject) => {
            this.__show();
            this.__updateFileList();
            const intervalTimer = setInterval(() => {
                if (!this.isShown()) {
                    clearInterval(intervalTimer);
                    resolve(this.__selectedFilename);
                }
            }, 50);
        });
    }

    getSelectedFilename() {
        return this.__selectedFilename;
    }

    show() {
        this.__show();
        this.__updateFileList();
    }
};


window.FileManagerUI = FileManagerUI;
