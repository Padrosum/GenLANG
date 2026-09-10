package genlang;

public final class GenLangException extends RuntimeException {
    public final int code;
    public final int line;
    public final int column;
    public final String path;

    public GenLangException(int code, String message) {
        this(code, message, 0, 0, null);
    }

    public GenLangException(int code, String message, int line, int column, String path) {
        super(line > 0
                ? (path == null || path.isEmpty() ? "<input>" : path) + ":" + line + ":" + column + ": " + message
                : (message == null ? "error" : message));
        this.code = code;
        this.line = line;
        this.column = column;
        this.path = path;
    }
}
