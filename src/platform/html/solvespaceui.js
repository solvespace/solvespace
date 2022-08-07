function isModal() {
    var hasModal = !!document.querySelector('.modal');
    var hasMenuBar = !!document.querySelector('.menubar .selected');
    var hasPopupMenu = !!document.querySelector('.menu.popup');
    var hasEditor = false;
    document.querySelectorAll('.editor').forEach(function(editor) {
        if(editor.style.display == "") {
            hasEditor = true;
        }
    });
    return hasModal || hasMenuBar || hasPopupMenu || hasEditor;
}

/* String helpers */

/** 
 * @param {string} s - original string
 * @param {number} digits - char length of generating string
 * @param {string} ch - string to be used for padding
 * @return {string} generated string ($digits chars length) or $s
 */
function stringPadLeft(s, digits, ch) {
    if (s.length > digits) {
        return s;
    }
    for (let i = s.length; i < digits; i++) {
        s = ch + s;
    }
    return s;
}

/** Generate a string expression of now
 * @return {string} like a "2022_08_31_2245" string (for 2022-08-31 22:45; local time)
 */
function GetCurrentDateTimeString() {
    const now = new Date();
    const padLeft2 = (num) => { return stringPadLeft(num.toString(), 2, '0') };
    return (`${now.getFullYear()}_${padLeft2(now.getMonth()+1)}_${padLeft2(now.getDate())}` +
            `_` + `${padLeft2(now.getHours())}${padLeft2(now.getMinutes())}`);
}

/* CSS helpers */
function hasClass(element, className) {
    return element.classList.contains(className);
}
function addClass(element, className) {
    element.classList.add(className);
}
function removeClass(element, className) {
    element.classList.remove(className);
}
function removeClassFromAllChildren(element, className) {
    element.querySelectorAll('.' + className).forEach(function(element) {
        removeClass(element, className);
    })
}

/* Mnemonic helpers */
function setLabelWithMnemonic(element, labelText) {
    var label = document.createElement('span');
    addClass(label, 'label');
    element.appendChild(label);

    var matches = labelText.match('(.*?)&(.)(.*)?');
    if(matches) {
        label.appendChild(document.createTextNode(matches[1]));
        if(matches[2]) {
            var mnemonic = document.createElement('u');
            mnemonic.innerText = matches[2];
            label.appendChild(mnemonic);
            addClass(element, 'mnemonic-Key' + matches[2].toUpperCase());
        }
        if(matches[3]) {
            label.appendChild(document.createTextNode(matches[3]));
        }
    } else {
        label.appendChild(document.createTextNode(labelText))
    }
}

/* Button helpers */
function isButton(element) {
    return hasClass(element, 'button');
}

/* Button DOM traversal helpers */
function getButton(element) {
    if(!element) return;
    if(element.tagName == 'U') {
        element = element.parentElement;
    }
    if(hasClass(element, 'label')) {
        return getButton(element.parentElement);
    } else if(isButton(element)) {
        return element;
    }
}

/* Button behavior */
window.addEventListener('click', function(event) {
    var button = getButton(event.target);
    if(button) {
        button.dispatchEvent(new Event('trigger'));
    }
});

window.addEventListener('keydown', function(event) {
    var selected = document.querySelector('.button.selected');
    if(!selected) return;

    var outSelected, newSelected;
    if(event.key == 'ArrowRight') {
        outSelected = selected;
        newSelected = selected.nextElementSibling;
        if(!newSelected) {
            newSelected = outSelected.parentElement.firstElementChild;
        }
    } else if(event.key == 'ArrowLeft') {
        outSelected = selected;
        newSelected = selected.previousElementSibling;
        if(!newSelected) {
            newSelected = outSelected.parentElement.lastElementChild;
        }
    } else if(event.key == 'Enter') {
        selected.dispatchEvent(new Event('trigger'));
    } else if(event.key == 'Escape' && hasClass(selected, 'default')) {
        selected.dispatchEvent(new Event('trigger'));
    }

    if(outSelected) removeClass(outSelected, 'selected');
    if(newSelected) addClass(newSelected, 'selected');

    event.stopPropagation();
});

/* Editor helpers */
function isEditor(element) {
    return hasClass(element, 'editor');
}

/* Editor DOM traversal helpers */
function getEditor(element) {
    if(!element) return;
    if(isEditor(element)) {
        return element;
    }
}

/* Editor behavior */
window.addEventListener('keydown', function(event) {
    var editor = getEditor(event.target);
    if(editor) {
        if(event.key == 'Enter') {
            editor.dispatchEvent(new Event('trigger'));
        } else if(event.key == 'Escape') {
            editor.style.display = 'none';
        }
        event.stopPropagation();
    }
}, {capture: true});

/* Menu helpers */
function isMenubar(element) {
    return hasClass(element, 'menubar');
}
function isMenu(element) {
    return hasClass(element, 'menu');
}
function isPopupMenu(element) {
    return isMenu(element) && hasClass(element, 'popup')
}
function hasSubmenu(menuItem) {
    return !!menuItem.querySelector('.menu');
}

/* Menu item helpers */
function isMenuItemSelectable(menuItem) {
    return !(hasClass(menuItem, 'disabled') || hasClass(menuItem, 'separator'));
}
function isMenuItemSelected(menuItem) {
    return hasClass(menuItem, 'selected') || hasClass(menuItem, 'hover');
}
function deselectMenuItem(menuItem) {
    removeClass(menuItem, 'selected');
    removeClass(menuItem, 'hover');
    removeClassFromAllChildren(menuItem, 'selected');
    removeClassFromAllChildren(menuItem, 'hover');
}
function selectMenuItem(menuItem) {
    var menu = menuItem.parentElement;
    removeClassFromAllChildren(menu, 'selected');
    removeClassFromAllChildren(menu, 'hover');
    if(isMenubar(menu)) {
        addClass(menuItem, 'selected');
    } else {
        addClass(menuItem, 'hover');
    }
}
function triggerMenuItem(menuItem) {
    selectMenuItem(menuItem);
    if(hasSubmenu(menuItem)) {
        selectMenuItem(menuItem.querySelector('li:first-child'));
    } else {
        var parent = menuItem.parentElement;
        while(!isMenubar(parent) && !isPopupMenu(parent)) {
            parent = parent.parentElement;
        }
        removeClassFromAllChildren(parent, 'selected');
        removeClassFromAllChildren(parent, 'hover');
        if(isPopupMenu(parent)) {
            parent.remove();
        }

        menuItem.dispatchEvent(new Event('trigger'));
    }
}

/* Menu DOM traversal helpers */
function getMenuItem(element) {
    if(!element) return;
    if(element.tagName == 'U') {
        element = element.parentElement;
    }
    if(hasClass(element, 'label')) {
        return getMenuItem(element.parentElement);
    } else if(element.tagName == 'LI' && isMenu(element.parentElement)) {
        return element;
    }
}
function getMenu(element) {
    if(!element) return;
    if(isMenu(element)) {
        return element;
    } else {
        var menuItem = getMenuItem(element);
        if(menuItem && isMenu(menuItem.parentElement)) {
            return menuItem.parentElement;
        }
    }
}

/* Menu behavior */
window.addEventListener('click', function(event) {
    var menuItem = getMenuItem(event.target);
    var menu = getMenu(menuItem);
    if(menu && isMenubar(menu)) {
        if(hasClass(menuItem, 'selected')) {
            removeClass(menuItem, 'selected');
        } else {
            selectMenuItem(menuItem);
        }
        event.stopPropagation();
    } else if(menu) {
        if(!hasSubmenu(menuItem)) {
            triggerMenuItem(menuItem);
        }
        event.stopPropagation();
    } else {
        document.querySelectorAll('.menu .selected, .menu .hover')
                .forEach(function(menuItem) {
            deselectMenuItem(menuItem);
            event.stopPropagation();
        });
        document.querySelectorAll('.menu.popup')
                .forEach(function(menu) {
            menu.remove();
        });
    }
});
window.addEventListener('mouseover', function(event) {
    var menuItem = getMenuItem(event.target);
    var menu = getMenu(menuItem);
    if(menu) {
        var selected = menu.querySelectorAll('.selected, .hover');
        if(isMenubar(menu)) {
            if(selected.length > 0) {
                selected.forEach(function(menuItem) {
                    if(selected != menuItem) {
                        deselectMenuItem(menuItem);
                    }
                });
                addClass(menuItem, 'selected');
            }
        } else {
            if(isMenuItemSelectable(menuItem)) {
                selectMenuItem(menuItem);
            }
        }
    }
});
window.addEventListener('keydown', function(event) {
    var allSelected = document.querySelectorAll('.menubar .selected, .menubar .hover,' +
                                                '.menu.popup .selected, .menu.popup .hover');
    if(allSelected.length == 0) return;

    var selected = allSelected[allSelected.length - 1];
    var outSelected, newSelected;
    var isMenubarItem = isMenubar(getMenu(selected));

    if(isMenubarItem && event.key == 'ArrowRight' ||
            !isMenubarItem && event.key == 'ArrowDown') {
        outSelected = selected;
        newSelected = selected.nextElementSibling;
        while(newSelected && !isMenuItemSelectable(newSelected)) {
            newSelected = newSelected.nextElementSibling;
        }
        if(!newSelected) {
            newSelected = outSelected.parentElement.firstElementChild;
        }
    } else if(isMenubarItem && event.key == 'ArrowLeft' ||
                !isMenubarItem && event.key == 'ArrowUp') {
        outSelected = selected;
        newSelected = selected.previousElementSibling;
        while(newSelected && !isMenuItemSelectable(newSelected)) {
            newSelected = newSelected.previousElementSibling;
        }
        if(!newSelected) {
            newSelected = outSelected.parentElement.lastElementChild;
        }
    } else if(!isMenubarItem && event.key == 'ArrowRight') {
        if(hasSubmenu(selected)) {
            selectMenuItem(selected.querySelector('li:first-child'));
        } else {
            outSelected = allSelected[0];
            newSelected = outSelected.nextElementSibling;
            if(!newSelected) {
                newSelected = outSelected.parentElement.firstElementChild;
            }
        }
    } else if(!isMenubarItem && event.key == 'ArrowLeft') {
        if(allSelected.length > 2) {
            outSelected = selected;
        } else {
            outSelected = allSelected[0];
            newSelected = outSelected.previousElementSibling;
            if(!newSelected) {
                newSelected = outSelected.parentElement.lastElementChild;
            }
        }
    } else if(isMenubarItem && event.key == 'ArrowDown') {
        newSelected = selected.querySelector('li:first-child');
    } else if(event.key == 'Enter') {
        triggerMenuItem(selected);
    } else if(event.key == 'Escape') {
        outSelected = allSelected[0];
    } else {
        var withMnemonic = getMenu(selected).querySelector('.mnemonic-' + event.key);
        if(withMnemonic) {
            triggerMenuItem(withMnemonic);
        }
    }

    if(outSelected) deselectMenuItem(outSelected);
    if(newSelected) selectMenuItem(newSelected);

    event.stopPropagation();
});

/* Mnemonic behavior */
window.addEventListener('keydown', function(event) {
    var withMnemonic;
    if(event.altKey && event.key == 'Alt') {
        addClass(document.body, 'mnemonic');
    } else if(!isModal() && event.altKey && (withMnemonic =
                document.querySelector('.menubar > .mnemonic-' + event.code))) {
        triggerMenuItem(withMnemonic);
        event.stopPropagation();
    } else {
        removeClass(document.body, 'mnemonic');
    }
});
window.addEventListener('keyup', function(event) {
    if(event.key == 'Alt') {
       removeClass(document.body, 'mnemonic');
    }
});


// FIXME(emscripten): Should be implemnted in guihtmlcpp ?
class FileUploadHelper {
    constructor() {
        this.modalRoot = document.createElement("div");
        addClass(this.modalRoot, "modal");
        this.modalRoot.style.display = "none";
        this.modalRoot.style.zIndex = 1000;
        
        this.dialogRoot = document.createElement("div");
        addClass(this.dialogRoot, "dialog");
        this.modalRoot.appendChild(this.dialogRoot);

        this.messageHeader = document.createElement("strong");
        this.dialogRoot.appendChild(this.messageHeader);

        this.descriptionParagraph = document.createElement("p");
        this.dialogRoot.appendChild(this.descriptionParagraph);

        this.currentFileListHeader = document.createElement("p");
        this.currentFileListHeader.textContent = "Current uploaded files:";
        this.dialogRoot.appendChild(this.currentFileListHeader);

        this.currentFileList = document.createElement("div");
        this.dialogRoot.appendChild(this.currentFileList);

        this.fileInputContainer = document.createElement("div");

        this.fileInputElement = document.createElement("input");
        this.fileInputElement.setAttribute("type", "file");
        this.fileInputElement.addEventListener("change", (ev)=> this.onFileInputChanged(ev));
        this.fileInputContainer.appendChild(this.fileInputElement);

        this.dialogRoot.appendChild(this.fileInputContainer);

        this.buttonHolder = document.createElement("div");
        addClass(this.buttonHolder, "buttons");
        this.dialogRoot.appendChild(this.buttonHolder);

        this.AddButton("OK", 0, false);
        this.AddButton("Cancel", 1, true);
        
        this.closeDialog();

        document.querySelector("body").appendChild(this.modalRoot);

        this.currentFilename = null;

        // FIXME(emscripten): For debugging
        this.title = "";
        this.filename = "";
        this.filters = "";
    }

    dispose() {
        document.querySelector("body").removeChild(this.modalRoot);
    }

    AddButton(label, response, isDefault) {
        // FIXME(emscripten): implement
        const buttonElem = document.createElement("div");
        addClass(buttonElem, "button");
        setLabelWithMnemonic(buttonElem, label);
        if (isDefault) {
            addClass(buttonElem, "default");
            addClass(buttonElem, "selected");
        }
        buttonElem.addEventListener("click", () => {
            this.closeDialog();
        });

        this.buttonHolder.appendChild(buttonElem);
    }

    getFileEntries() {
        const basePath = '/';
        /** @type {Array<object} */
        const nodes = FS.readdir(basePath);
        const files = nodes.filter((nodename) => {
            return FS.isFile(FS.lstat(basePath + nodename).mode);
        }).map((filename) => {
            return basePath + filename;
        });
        return files;
    }

    generateFileList() {
        let filepaths = this.getFileEntries();
        const listElem = document.createElement("ul");
        for (let i = 0; i < filepaths.length; i++) {
            const listitemElem = document.createElement("li");
            const stat = FS.lstat(filepaths[i]);
            const text = `"${filepaths[i]}" (${stat.size} bytes)`;
            listitemElem.textContent = text;
            listElem.appendChild(listitemElem);
        }
        return listElem;
    }

    updateFileList() {
        this.currentFileList.innerHTML = "";
        this.currentFileList.appendChild(this.generateFileList());
    }

    onFileInputChanged(ev) {
        const selectedFiles = ev.target.files;
        if (selectedFiles.length < 1) {
            return;
        }
        const selectedFile = selectedFiles[0];
        const selectedFilename = selectedFile.name;
        this.filename = selectedFilename;
        this.currentFilename = selectedFilename;

        // Prepare FileReader
        const fileReader = new FileReader();
        const fileReaderReadAsArrayBufferPromise = new Promise((resolve, reject) => {
            fileReader.addEventListener("load", (ev) => {
                resolve(ev.target.result);
            });
            fileReader.addEventListener("abort", (err) => {
                reject(err);
            });
            fileReader.readAsArrayBuffer(selectedFile);
        });

        fileReaderReadAsArrayBufferPromise
            .then((arrayBuffer) => {
                // Write selected file to FS
                console.log(`Write uploaded file blob to filesystem. "${selectedFilename}" (${arrayBuffer.byteLength} bytes)`);
                const u8array = new Uint8Array(arrayBuffer);
                const fs = FS.open("/" + selectedFilename, "w");
                FS.write(fs, u8array, 0, u8array.length, 0);
                FS.close(fs);

                // Update file list in dialog
                this.updateFileList();
            })
            .catch((err) => {
                console.error("Error while fileReader.readAsArrayBuffer():", err);
            });
    }

    showDialog() {
        this.updateFileList();

        this.is_shown = true;
        this.modalRoot.style.display = "block";
    }

    closeDialog() {
        this.is_shown = false;
        this.modalRoot.style.display = "none";
    }
};

// FIXME(emscripten): Workaround
function createFileUploadHelperInstance() {
    return new FileUploadHelper();
}

// FIXME(emscripten): Should be implemnted in guihtmlcpp ?
class FileDownloadHelper {
    constructor() {
        this.modalRoot = document.createElement("div");
        addClass(this.modalRoot, "modal");
        this.modalRoot.style.display = "none";
        this.modalRoot.style.zIndex = 1000;
        
        this.dialogRoot = document.createElement("div");
        addClass(this.dialogRoot, "dialog");
        this.modalRoot.appendChild(this.dialogRoot);

        this.messageHeader = document.createElement("strong");
        this.dialogRoot.appendChild(this.messageHeader);

        this.descriptionParagraph = document.createElement("p");
        this.dialogRoot.appendChild(this.descriptionParagraph);

        this.buttonHolder = document.createElement("div");
        addClass(this.buttonHolder, "buttons");
        this.dialogRoot.appendChild(this.buttonHolder);
        
        this.closeDialog();

        document.querySelector("body").appendChild(this.modalRoot);
    }

    dispose() {
        document.querySelector("body").removeChild(this.modalRoot);
    }

    AddButton(label, response, isDefault) {
        // FIXME(emscripten): implement
        const buttonElem = document.createElement("div");
        addClass(buttonElem, "button");
        setLabelWithMnemonic(buttonElem, label);
        if (isDefault) {
            addClass(buttonElem, "default");
            addClass(buttonElem, "selected");
        }
        buttonElem.addEventListener("click", () => {
            this.closeDialog();
            this.dispose();
        });

        this.buttonHolder.appendChild(buttonElem);
    }

    createBlobURLFromArrayBuffer(arrayBuffer) {
        const u8array = new Uint8Array(arrayBuffer);
        let dataUrl = "data:application/octet-stream;base64,";
        let binaryString = "";
        for (let i = 0; i < u8array.length; i++) {
            binaryString += String.fromCharCode(u8array[i]);
        }
        dataUrl += btoa(binaryString);

        return dataUrl;
    }

    prepareFile(filename) {
        this.messageHeader.textContent = "Your file ready";

        const stat = FS.lstat(filename);
        const filesize = stat.size;
        const fs = FS.open(filename, "r");
        const readbuffer = new Uint8Array(filesize);
        FS.read(fs, readbuffer, 0, filesize, 0);
        FS.close(fs);

        const blobURL = this.createBlobURLFromArrayBuffer(readbuffer.buffer);

        this.descriptionParagraph.innerHTML = "";
        const linkElem = document.createElement("a");
        let downloadfilename = "solvespace_browser-";
        downloadfilename += `${GetCurrentDateTimeString()}.slvs`;
        linkElem.setAttribute("download", downloadfilename);
        linkElem.setAttribute("href", blobURL);
        // WORKAROUND: FIXME(emscripten)
        linkElem.style.color = "lightblue";
        linkElem.textContent = downloadfilename;
        this.descriptionParagraph.appendChild(linkElem);
    }

    showDialog() {
        this.is_shown = true;
        this.modalRoot.style.display = "block";
    }

    closeDialog() {
        this.is_shown = false;
        this.modalRoot.style.display = "none";
    }
};

function saveFileDone(filename, isSaveAs, isAutosave) {
    console.log(`saveFileDone(${filename}, ${isSaveAs}, ${isAutosave})`);
    if (isAutosave) {
        return;
    }
    const fileDownloadHelper = new FileDownloadHelper();
    fileDownloadHelper.AddButton("OK", 0, true);
    fileDownloadHelper.prepareFile(filename);
    console.log(`Calling shoDialog()...`);
    fileDownloadHelper.showDialog();
    console.log(`shoDialog() finished.`);
}
