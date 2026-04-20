from datetime import datetime

from build_hash import git_rev as _git_rev

def main():
	git_rev = _git_rev()
	now = datetime.now()
	print(f'''
#define BUILD_REV "{git_rev}"
#define BUILD_DATE "{now.isoformat()}"
#define BUILD_DATE_SHORT "{now.strftime('%m/%d/%y')}"
const char *build_rev = BUILD_REV;
const char *build_date = BUILD_DATE;
const char *build_ui_widget_text = BUILD_REV " " BUILD_DATE_SHORT;
''')

if __name__ == '__main__':
	main()

