import json

from flask import Flask, url_for, session
from flask import request, render_template
from flask_sqlalchemy import SQLAlchemy

import sys
import os

# getting the name of the directory
# where the this file is present.
current = os.path.dirname(os.path.realpath(__file__))

# Getting the parent directory name
# where the current directory is present.
parent = os.path.dirname(current)

# adding the parent directory to
# the sys.path.
sys.path.append(parent)

# now we can import the module in the parent
# directory.
import service.service as service
import model.models as model
import constants.constants as constants

app = Flask(__name__)

app.config.update(
    SECRET_KEY='root',
    SQLALCHEMY_DATABASE_URI='postgresql://postgres:root@localhost/my_db',
    SQLALCHEMY_TRACK_MODIFICATIONS=False
)

db = SQLAlchemy(app)
service_obj = service.Service(db)

""" DE PUS LOCK PE ACCESUL LA BAZA DE DATA !!!!"""


def check_if_admin():
    if 'id' not in session:
        return False
    user = service_obj.get_user_by_id(session['id'])
    if user.role != 1:
        return False
    return True


@app.route('/', methods=['GET'])
def welcome():
    return render_template('welcome_page.html')


@app.route('/login/<int:sign_case>', methods=['GET', 'POST'])
def login(sign_case):
    if request.method == 'GET':
        return render_template('login.html', case=sign_case, flask=Flask)
    else:
        form = request.form
        result = None

        if sign_case == 0:
            # sign up
            result = service_obj.register(form.get('username'), form.get('psw'), form.get('psw-repeat'))
        else:
            # sign in
            result = service_obj.login(form.get('username'), form.get('psw'))

        # unsuccessful cases
        if result == constants.UNSUCCESSFUL:
            return render_template('login.html', case=sign_case, message='Wrong username or password.')
        if result == constants.NOT_ALLOWED_INPUT:
            return render_template('login.html', case=sign_case,
                                   message='Some of the characters used in username field are not allowed.')

        # successful cases
        session['id'] = service_obj.find_by_username(form.get('username')).ID
        if result == constants.ADMIN:
            return render_template('admin_main.html')
        if result == constants.USER:
            return user_page(None, None)


@app.route('/admin_display', methods=['GET'])
def admin_display():
    return render_template('admin_main.html')


@app.route('/admin<string:scenario>', methods=['POST'])
def admin_page(scenario):
    if not check_if_admin():
        return render_template('welcome_page.html')

    action = 'update'
    if scenario == 'user' and request.form.get('option_user') != 'update':
        action = 'delete' if request.form.get('option_user') == 'delete' else 'create'
    elif scenario == 'device' and request.form.get('option_devices') != 'update':
        action = 'delete' if request.form.get('option_devices') == 'delete' else 'create'
    elif scenario == 'mapping':
        # mappings can only be updates
        pass

    message = ''
    if action == 'delete':
        service_obj.delete_entry(request.form, scenario)
    elif action == 'update':
        service_obj.handle_admin_post(request.form, scenario)
    elif action == 'create':
        status = service_obj.insert_entry(request.form, scenario)
        if status == constants.UNSUCCESSFUL:
            message = 'Some fields were left empty for insert operation.'
        elif status == constants.USER_NOT_FOUND:
            message = 'The username you entered was not found.'
    return render_template('admin_main.html', message=message)


@app.route('/showUsers', methods=['GET'])
def get_users():
    if not check_if_admin():
        return render_template('welcome_page.html')
    fields, users_list = service_obj.get_users_as_tuple_list()
    if len(users_list) == 0:
        return '<p>There are no users</p>'
    return render_template('display_table.html', fields=fields, len_fields=len(fields), content=users_list,
                           content_len=len(users_list))


@app.route('/showDevices', methods=['GET'])
def get_devices():
    if not check_if_admin():
        return render_template('welcome_page.html')
    fields, device_list = service_obj.users_join_devices_join_address()
    if len(device_list) == 0:
        return '<p>There are no devices.</p>'
    return render_template('display_table.html', fields=fields, len_fields=len(fields), content=device_list,
                           content_len=len(device_list))


@app.route('/main_page/<int:month_int>/<int:year>', methods=['GET'])
def user_page(month_int, year):
    if 'id' not in session:
        return 'Must be logged in.'
    user = service_obj.get_user_by_id(session['id'])
    fields, devices = service_obj.get_devices_by_user_id(user.ID)

    if month_int is None or year is None:
        month_int, year = service.get_current_date()

    month = constants.month_int_to_string[month_int]
    days = service_obj.get_days_in_month(year, month)
    if days is None:
        return '<p>No data for this month</p>'
    print(days, month, year)

    return render_template('user_main.html',
                           days=days, month=month, year=year, month_int=month_int,
                           username=user.username, index_device_id=constants.index_device,
                           len_fields=len(fields), fields=fields, content_len=len(devices), content=devices)


@app.route('/chart/<int:day>/<string:month>/<int:year>', methods=['GET'])
def show_chart(day, month, year):
    if 'id' not in session:
        return 'Must be logged in.'
    devices = service_obj.get_devices_consumption_for_a_user_id(session['id'], day, constants.month_string_to_int[month], year)
    data, labels = [], []
    for device_id, consumption in devices:
        labels.append(f'Device {device_id}')
        data.append(consumption)
    print(data, labels)

    return render_template('chart.html', day=day, month=month, year=year, month_int=constants.month_string_to_int[month],
                           my_data=json.dumps(data), labels=json.dumps(labels))


@app.route('/test', methods=['GET'])
def test():
    city = 'Satu Mare'
    country = 'Romania'
    return str(service_obj.add_address(city, country))
