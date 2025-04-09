R"css(
/* Main window styling */
window.solvespace-window {
    background-color: @theme_bg_color;
    color: @theme_fg_color;
}

window.solvespace-window.dark {
    background-color: #303030;
    color: #e0e0e0;
}

window.solvespace-window.light {
    background-color: #f0f0f0;
    color: #303030;
}

/* RTL text support */
window.solvespace-window[text-direction="rtl"] {
    direction: rtl;
}

window.solvespace-window[text-direction="rtl"] * {
    text-align: right;
}

/* Scrollbar styling */
scrollbar {
    background-color: alpha(@theme_fg_color, 0.1);
    border-radius: 0;
}

scrollbar slider {
    min-width: 16px;
    border-radius: 8px;
    background-color: alpha(@theme_fg_color, 0.3);
}

scrollbar slider:hover {
    background-color: alpha(@theme_fg_color, 0.5);
}

scrollbar slider:active {
    background-color: alpha(@theme_fg_color, 0.7);
}

/* GL area styling */
.solvespace-gl-area {
    background-color: @theme_base_color;
    border-radius: 2px;
    border: 1px solid @borders;
}

/* Menu button styling */
button.menu-button {
    padding: 4px 8px;
    border-radius: 3px;
    background-color: alpha(@theme_fg_color, 0.05);
    color: @theme_fg_color;
}

button.menu-button:hover {
    background-color: alpha(@theme_fg_color, 0.1);
}

button.menu-button:active {
    background-color: alpha(@theme_fg_color, 0.15);
}

/* Header styling */
.solvespace-header {
    padding: 4px;
    background-color: @theme_bg_color;
    border-bottom: 1px solid @borders;
}

/* Editor text styling */
.solvespace-editor-text {
    background-color: @theme_base_color;
    color: @theme_text_color;
    border-radius: 3px;
    padding: 4px;
    caret-color: @link_color;
}
)css"
